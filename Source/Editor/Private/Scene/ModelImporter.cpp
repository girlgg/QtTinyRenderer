#include "Scene/ModelImporter.h"

#include <io/qfileinfo.h>
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <assimp/matrix4x4.h>
#include <io/qdir.h>

#include "CommonRender.h"
#include "Component/MaterialComponent.h"
#include "Component/MeshComponent.h"
#include "Component/RenderableComponent.h"
#include "Component/TransformComponent.h"
#include "Resources/ResourceManager.h"
#include "Scene/World.h"

ModelImporter::ModelImporter(QSharedPointer<World> world, QSharedPointer<ResourceManager> resourceManager,
                             QObject *parent)
    : QObject(parent), mWorld(world), mResourceManager(resourceManager) {
    if (!mWorld || !mResourceManager) {
        qCritical("ModelImporter created with null World or ResourceManager!");
    }
}

EntityID ModelImporter::importModel(const QString &filePath) {
    if (!mWorld || !mResourceManager) {
        emit importFailed("Importer not initialized correctly.");
        return INVALID_ENTITY;
    }

    Assimp::Importer importer;
    const aiScene *scene = importer.ReadFile(filePath.toStdString(),
                                             aiProcess_Triangulate |
                                             aiProcess_GenSmoothNormals |
                                             aiProcess_FlipUVs |
                                             aiProcess_CalcTangentSpace |
                                             aiProcess_JoinIdenticalVertices |
                                             aiProcess_SortByPType
    );

    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
        emit importFailed(QString("Assimp Error: %1").arg(importer.GetErrorString()));
        return INVALID_ENTITY;
    }

    mCurrentModelBasePath = QFileInfo(filePath).absolutePath();
    qInfo() << "Importing model:" << filePath << "Base path:" << mCurrentModelBasePath;

    EntityID rootEntity = mWorld->createEntity();
    mWorld->addComponent<TransformComponent>(rootEntity, {});
    // mWorld->addComponent<NameComponent>(rootEntity, { QFileInfo(filePath).baseName() });
    // mWorld->addComponent<RenderableComponent>(rootEntity, {false});

    processNode(scene->mRootNode, scene, mCurrentModelBasePath, QMatrix4x4(), rootEntity);

    qInfo() << "Model import finished for:" << filePath;
    emit modelImportedSuccessfully();
    return rootEntity;
}

QMatrix4x4 ModelImporter::aiMatrix4x4ToQMatrix4x4(const aiMatrix4x4 &from) {
    return QMatrix4x4(
        from.a1, from.a2, from.a3, from.a4,
        from.b1, from.b2, from.b3, from.b4,
        from.c1, from.c2, from.c3, from.c4,
        from.d1, from.d2, from.d3, from.d4
    ).transposed();
}

void ModelImporter::processNode(aiNode *node, const aiScene *scene, const QString &modelDir,
                                const QMatrix4x4 &parentTransform, EntityID parentEntity) {
    if (!node) return;

    QMatrix4x4 nodeTransform = parentTransform * aiMatrix4x4ToQMatrix4x4(node->mTransformation);
    QString nodeName = QString::fromUtf8(node->mName.C_Str());
    qDebug() << "Processing Node:" << nodeName << "with" << node->mNumMeshes << "meshes and" << node->mNumChildren <<
            "children";

    EntityID currentNodeEntity = parentEntity;
    bool createdNodeEntity = false;
    if (node->mNumMeshes > 0 || node->mNumChildren > 0) {
        if (node != scene->mRootNode) {
            currentNodeEntity = mWorld->createEntity();
            createdNodeEntity = true;
            mWorld->addComponent<TransformComponent>(currentNodeEntity, {});
            // mWorld->addComponent<NameComponent>(currentNodeEntity, {nodeName});
            mWorld->addComponent<RenderableComponent>(currentNodeEntity, {{}, node->mNumMeshes > 0});

            TransformComponent *tf = mWorld->getComponent<TransformComponent>(currentNodeEntity);
            if (tf) {
                QMatrix4x4 localTransform = aiMatrix4x4ToQMatrix4x4(node->mTransformation);
                tf->setPosition(localTransform.column(3).toVector3D());
                QQuaternion rotation = QQuaternion::fromRotationMatrix(localTransform.normalMatrix());
                tf->setRotation(rotation);
            }
            // TODO: 添加父节点组件连接 Node 实体
        } else {
            TransformComponent *tf = mWorld->getComponent<TransformComponent>(currentNodeEntity);
            if (tf) {
                QMatrix4x4 localTransform = aiMatrix4x4ToQMatrix4x4(node->mTransformation);
                tf->setPosition(localTransform.column(3).toVector3D());
            }
        }
    }

    for (unsigned int i = 0; i < node->mNumMeshes; ++i) {
        aiMesh *mesh = scene->mMeshes[node->mMeshes[i]];
        EntityID meshEntity = processMesh(mesh, scene, modelDir, nodeTransform,
                                          nodeName + "_mesh_" + QString::number(i));
        if (meshEntity != INVALID_ENTITY) {
            if (createdNodeEntity) {
                // TODO: 添加父节点组件连接 Node 实体
            }
            TransformComponent *meshTf = mWorld->getComponent<TransformComponent>(meshEntity);
            if (meshTf) {
                meshTf->setPosition(nodeTransform.column(3).toVector3D());
                QQuaternion rot = QQuaternion::fromRotationMatrix(nodeTransform.normalMatrix());
                meshTf->setRotation(rot);
            }
        }
    }

    for (unsigned int i = 0; i < node->mNumChildren; ++i) {
        processNode(node->mChildren[i], scene, modelDir, nodeTransform, currentNodeEntity);
    }
}

EntityID ModelImporter::processMesh(aiMesh *mesh, const aiScene *scene, const QString &modelDir,
                                    const QMatrix4x4 &nodeTransform, const QString &baseName) {
    if (!mesh || mesh->mNumVertices == 0 || mesh->mNumFaces == 0) {
        qWarning() << "Skipping empty or invalid mesh:" << baseName;
        return INVALID_ENTITY;
    }

    qDebug() << "Processing Mesh:" << baseName << "Vertices:" << mesh->mNumVertices << "Faces:" << mesh->mNumFaces;

    QVector<VertexData> vertices;
    vertices.reserve(mesh->mNumVertices);
    QVector<quint16> indices;
    indices.reserve(mesh->mNumFaces * 3);

    // --- 处理顶点 ---
    bool hasNormals = mesh->HasNormals();
    bool hasTexCoords = mesh->HasTextureCoords(0);
    bool hasTangentsAndBitangents = mesh->HasTangentsAndBitangents();
    for (unsigned int i = 0; i < mesh->mNumVertices; ++i) {
        VertexData vertex;
        vertex.position = QVector3D(mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z);

        if (hasNormals) {
            vertex.normal = QVector3D(mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z);
        } else {
            vertex.normal = QVector3D(0.0f, 1.0f, 0.0f);
        }

        if (hasTexCoords && mesh->mTextureCoords[0]) {
            vertex.texCoord = QVector2D(mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y);
        } else {
            vertex.texCoord = QVector2D(0.0f, 0.0f); // Default UV
        }
        if (hasTangentsAndBitangents) {
            vertex.tangent = QVector3D(mesh->mTangents[i].x, mesh->mTangents[i].y, mesh->mTangents[i].z);
        }
        vertices.append(vertex);
    }

    // --- 处理索引 ---
    for (unsigned int i = 0; i < mesh->mNumFaces; ++i) {
        aiFace face = mesh->mFaces[i];
        if (face.mNumIndices != 3) {
            qWarning() << "Skipping non-triangular face in mesh:" << baseName;
            continue;
        }
        for (unsigned int j = 0; j < face.mNumIndices; ++j) {
            indices.append(static_cast<quint16>(face.mIndices[j]));
        }
    }

    if (vertices.isEmpty() || indices.isEmpty()) {
        qWarning() << "Mesh processing resulted in empty vertices or indices for:" << baseName;
        return INVALID_ENTITY;
    }

    // --- 创建实体组件 ---
    EntityID entity = mWorld->createEntity();
    // mWorld->addComponent<NameComponent>(entity, {baseName});

    // --- Mesh Component ---
    QString meshResourceId = QString("imported_%1").arg(baseName);
    mResourceManager->loadMeshFromData(meshResourceId, vertices, indices);
    MeshComponent meshComp;
    meshComp.meshResourceId = meshResourceId;
    mWorld->addComponent<MeshComponent>(entity, meshComp);

    // --- Material Component ---
    MaterialComponent matComp;
    if (mesh->mMaterialIndex >= 0) {
        aiMaterial *material = scene->mMaterials[mesh->mMaterialIndex];
        aiString texPath;
        QString fullPath;

        auto findTexture = [&](aiTextureType type, const char *logName) -> QString {
            if (material->GetTextureCount(type) > 0) {
                if (material->GetTexture(type, 0, &texPath) == AI_SUCCESS) {
                    QString pathStr = QString::fromUtf8(texPath.C_Str());
                    fullPath = QDir(modelDir).filePath(pathStr);
                    if (QFile::exists(fullPath)) {
                        qDebug() << "Found" << logName << "Texture:" << fullPath << "(Type:" << type << ")";
                        return fullPath;
                    }
                    qWarning() << logName << "texture file not found at primary path:" << fullPath;
                    QString altPath = pathStr;
                    if (QFile::exists(altPath)) {
                        qDebug() << "  Found" << logName << "texture at alternate path:" << altPath;
                        return altPath;
                    }
                    qWarning() << "  " << logName << "texture also not found at alternate path:" << altPath;
                } else {
                    qWarning() << "Assimp GetTexture failed for type" << type;
                }
            }
            return QString();
        };
        // Albedo / Diffuse
        matComp.albedoMapResourceId = findTexture(aiTextureType_DIFFUSE, "Diffuse");
        if (matComp.albedoMapResourceId.isEmpty()) {
            matComp.albedoMapResourceId = DEFAULT_WHITE_TEXTURE_ID;
            aiColor4D diffuseColor;
            if (aiGetMaterialColor(material, AI_MATKEY_COLOR_DIFFUSE, &diffuseColor) == AI_SUCCESS) {
                matComp.albedoFactor = QVector3D(diffuseColor.r, diffuseColor.g, diffuseColor.b);
            }
        }
        // Normal (Try multiple types)
        matComp.normalMapResourceId = findTexture(aiTextureType_NORMALS, "Normal");
        if (matComp.normalMapResourceId.isEmpty()) {
            matComp.normalMapResourceId = findTexture(aiTextureType_HEIGHT, "Height (as Normal)");
        }
        if (matComp.normalMapResourceId.isEmpty()) {
            matComp.normalMapResourceId = findTexture(aiTextureType_NORMAL_CAMERA, "NormalCamera");
        }
        if (matComp.normalMapResourceId.isEmpty()) {
            matComp.normalMapResourceId = DEFAULT_NORMAL_MAP_ID;
        }
        // --- PBR Metallic/Roughness Texture Search ---
        matComp.metallicRoughnessMapResourceId = findTexture(aiTextureType_METALNESS,
                                                             "Metallic/Roughness (GLTF PBR)");
        if (matComp.metallicRoughnessMapResourceId.isEmpty()) {
            matComp.metallicRoughnessMapResourceId = findTexture(aiTextureType_DIFFUSE_ROUGHNESS,
                                                                 "Metallic/Roughness (GLTF PBR)");
            if (!matComp.metallicRoughnessMapResourceId.isEmpty()) {
                qDebug() << "  Found Roughness map, potentially combined Metal/Rough - using this ID.";
            }
        } else {
            qDebug() << "  Found Metalness map, potentially combined Metal/Rough - using this ID.";
        }
        // Fallback: Check traditional Specular/Shininess if PBR maps aren't found (Less ideal)
        if (matComp.metallicRoughnessMapResourceId.isEmpty()) {
            matComp.metallicRoughnessMapResourceId = findTexture(aiTextureType_SPECULAR, "Specular (Fallback)");
        }
        if (matComp.metallicRoughnessMapResourceId.isEmpty()) {
            matComp.metallicRoughnessMapResourceId = DEFAULT_METALROUGH_TEXTURE_ID;
        }
        // Also try reading factors
        float metallicFactor = 0.0f;
        float roughnessFactor = 1.0f;
        aiGetMaterialFloat(material, AI_MATKEY_METALLIC_FACTOR, &metallicFactor);
        aiGetMaterialFloat(material, AI_MATKEY_ROUGHNESS_FACTOR, &roughnessFactor);
        matComp.metallicFactor = metallicFactor;
        matComp.roughnessFactor = roughnessFactor;


        // Ambient Occlusion
        matComp.ambientOcclusionMapResourceId = findTexture(aiTextureType_AMBIENT_OCCLUSION, "AO");
        if (matComp.ambientOcclusionMapResourceId.isEmpty()) {
            // Sometimes AO is packed in LIGHTMAP
            matComp.ambientOcclusionMapResourceId = findTexture(aiTextureType_LIGHTMAP, "Lightmap (as AO)");
        }
        if (matComp.ambientOcclusionMapResourceId.isEmpty()) {
            matComp.ambientOcclusionMapResourceId = DEFAULT_WHITE_TEXTURE_ID; // Default white (no occlusion)
        }

        // Emissive
        matComp.emissiveMapResourceId = findTexture(aiTextureType_EMISSIVE, "Emissive");
        if (matComp.emissiveMapResourceId.isEmpty()) {
            matComp.emissiveMapResourceId = DEFAULT_BLACK_TEXTURE_ID;
        }
        aiColor4D emissiveColor;
        if (aiGetMaterialColor(material, AI_MATKEY_COLOR_EMISSIVE, &emissiveColor) == AI_SUCCESS) {
            matComp.emissiveFactor = QVector3D(emissiveColor.r, emissiveColor.g, emissiveColor.b);
        }
    } else {
        matComp.albedoMapResourceId = DEFAULT_WHITE_TEXTURE_ID;
        matComp.normalMapResourceId = DEFAULT_NORMAL_MAP_ID;
        matComp.metallicRoughnessMapResourceId = DEFAULT_METALROUGH_TEXTURE_ID;
        matComp.ambientOcclusionMapResourceId = DEFAULT_WHITE_TEXTURE_ID;
        matComp.emissiveMapResourceId = DEFAULT_BLACK_TEXTURE_ID;
    }
    QString materialCacheKey = mResourceManager->generateMaterialCacheKey(&matComp);
    if (!mResourceManager->getMaterialGpuData(materialCacheKey)) {
        mResourceManager->loadMaterial(materialCacheKey, &matComp);
    } else {
        qDebug() << "ProcessMesh: Material already cached:" << materialCacheKey;
    }
    mWorld->addComponent<MaterialComponent>(entity, matComp);
    mWorld->addComponent<TransformComponent>(entity, {});
    mWorld->addComponent<RenderableComponent>(entity, {{}, true});

    return entity;
}
