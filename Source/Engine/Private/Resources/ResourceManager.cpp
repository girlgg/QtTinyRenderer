#include "Resources/ResourceManager.h"

#include "Component/MaterialComponent.h"
#include "Component/MeshComponent.h"
#include <rhi/qrhi.h>

ResourceManager::~ResourceManager() {
    qInfo() << "ResourceManager destroyed. Releasing resources...";
    releaseRhiResources();
}

void ResourceManager::initialize(QSharedPointer<QRhi> rhi) {
    mRhi = rhi;
    if (!mRhi) {
        qCritical() << "ResourceManager initialized with a null QRhi pointer!";
        return;
    }
    loadDefaultResources();
    qInfo() << "ResourceManager initialized.";
}

void ResourceManager::loadDefaultResources() {
    createDefaultTextures();
}

void ResourceManager::createDefaultTextures() {
    if (!mRhi) return;

    auto createTexture = [&](const QString &id, const QImage &img) {
        if (mMaterialCache.contains(id)) return;

        RhiTextureGpuData texGpuData;
        texGpuData.texture.reset(mRhi->newTexture(QRhiTexture::RGBA8, img.size(), 1));
        if (texGpuData.texture && texGpuData.texture->create()) {
            texGpuData.texture->setName(id.toUtf8());
            texGpuData.sourceImage = img;
            texGpuData.ready = false;
            mTextureCache.insert(id, std::move(texGpuData));
        }
    };
    createTexture(DEFAULT_WHITE_TEXTURE_ID, QImage(WHITE_PIXEL, 1, 1, QImage::Format_RGBA8888));
    createTexture(DEFAULT_BLACK_TEXTURE_ID, QImage(BLACK_PIXEL, 1, 1, QImage::Format_RGBA8888));
    createTexture(DEFAULT_NORMAL_MAP_ID, QImage(NORMAL_PIXEL, 1, 1, QImage::Format_RGBA8888));
    createTexture(DEFAULT_METALROUGH_TEXTURE_ID, QImage(METALROUGH_DEFAULT_PIXEL, 1, 1, QImage::Format_RGBA8888));
}

void ResourceManager::releaseRhiResources() {
    mMeshCache.clear();
    mMaterialCache.clear();
    mRhi.clear();
}

void ResourceManager::loadMeshFromData(const QString &id, const QVector<VertexData> &vertices,
                                       const QVector<quint16> &indices) {
    if (mMeshCache.count(id) || !mRhi) return;

    RhiMeshGpuData gpuData;
    gpuData.vertexCount = vertices.size();
    gpuData.indexCount = indices.size();
    const quint32 vertexBufferSize = vertices.size() * sizeof(VertexData);
    const quint32 indexBufferSize = indices.size() * sizeof(quint16);

    // --- 矫正size ---
    gpuData.vertexBuffer.reset(mRhi->newBuffer(QRhiBuffer::Immutable,
                                               QRhiBuffer::VertexBuffer,
                                               vertexBufferSize));
    if (!gpuData.vertexBuffer || !gpuData.vertexBuffer->create()) {
        qWarning() << "Failed to create vertex buffer for" << id;
        return;
    }
    gpuData.vertexBuffer->setName(id.toUtf8() + "_VB");

    gpuData.indexBuffer.reset(mRhi->newBuffer(QRhiBuffer::Immutable, QRhiBuffer::IndexBuffer,
                                              indexBufferSize));
    if (!gpuData.indexBuffer || !gpuData.indexBuffer->create()) {
        qWarning() << "Failed to create index buffer for" << id;
        gpuData.vertexBuffer.reset();
        return;
    }
    gpuData.indexBuffer->setName(id.toUtf8() + "_IB");

    gpuData.sourceVertices = vertices;
    gpuData.sourceIndices = indices;
    gpuData.ready = false;

    mMeshCache.insert(id, std::move(gpuData));
}

RhiMeshGpuData *ResourceManager::getMeshGpuData(const QString &id) {
    if (mMeshCache.count(id))
        return &mMeshCache[id];
    return nullptr;
}

bool ResourceManager::queueMeshUpdate(const QString &id, QRhiResourceUpdateBatch *batch) {
    if (!mRhi || !batch) return false;
    RhiMeshGpuData *gpuData = getMeshGpuData(id);
    if (!gpuData) {
        qWarning() << "ResourceManager::queueMeshUpdate - Mesh GPU data for ID '" << id << "' not found.";
        return false;
    }

    if (gpuData->ready) {
        return true;
    }

    if (!gpuData->vertexBuffer || !gpuData->indexBuffer) {
        qWarning() << "ResourceManager::queueMeshUpdate - Buffers for mesh '" << id << "' are null. Cannot upload.";
        return false;
    }
    if (gpuData->sourceVertices.isEmpty() || gpuData->sourceIndices.isEmpty()) {
        qWarning() << "ResourceManager::queueMeshUpdate - Source vertex or index data missing for mesh '" << id <<
                "'. Cannot upload.";
        return false;
    }

    batch->uploadStaticBuffer(gpuData->vertexBuffer.get(), 0, gpuData->sourceVertices.size() * sizeof(VertexData),
                              gpuData->sourceVertices.constData());
    batch->uploadStaticBuffer(gpuData->indexBuffer.get(), 0, gpuData->sourceIndices.size() * sizeof(quint16),
                              gpuData->sourceIndices.constData());

    gpuData->ready = true;
    gpuData->sourceVertices.clear();
    gpuData->sourceVertices.squeeze();
    gpuData->sourceIndices.clear();
    gpuData->sourceIndices.squeeze();

    gpuData->ready = true;
    return true;
}

void ResourceManager::loadTexture(const QString &textureId) {
    if (textureId.isEmpty() || mTextureCache.contains(textureId)) return;
    if (!mRhi) {
        qWarning() << "ResourceManager::loadTexture - RHI not initialized.";
        return;
    }

    qInfo() << "ResourceManager: Loading texture description for:" << textureId;
    QImage image(textureId);
    if (image.isNull()) {
        qWarning() << "Failed to load image file:" << textureId << ". Using default white texture instead.";
        return;
    }
    image = image.convertToFormat(QImage::Format_RGBA8888);

    RhiTextureGpuData texGpuData;
    texGpuData.texture.reset(mRhi->newTexture(QRhiTexture::RGBA8, image.size(), 1));
    if (!texGpuData.texture || !texGpuData.texture->create()) {
        qWarning() << "Failed to create texture for" << textureId;
        return;
    }
    texGpuData.texture->setName(textureId.toUtf8());
    texGpuData.sourceImage = image;
    texGpuData.ready = false;
    mTextureCache.insert(textureId, std::move(texGpuData));
}

void ResourceManager::loadMaterial(const QString &materialId, const MaterialComponent *definition) {
    if (mMaterialCache.contains(materialId)) return; // Already defined
    if (!mRhi || !definition) {
        qWarning() << "ResourceManager::loadMaterial - Invalid RHI or definition for" << materialId;
        return;
    }
    qInfo() << "Defining material cache entry:" << materialId;
    RhiMaterialGpuData gpuData;
    gpuData.albedoId = definition->albedoMapResourceId.isEmpty()
                           ? DEFAULT_WHITE_TEXTURE_ID
                           : definition->albedoMapResourceId;
    gpuData.normalId = definition->normalMapResourceId.isEmpty()
                           ? DEFAULT_NORMAL_MAP_ID
                           : definition->normalMapResourceId;
    gpuData.metallicRoughnessId = definition->metallicRoughnessMapResourceId.isEmpty()
                                      ? DEFAULT_METALROUGH_TEXTURE_ID
                                      : definition->metallicRoughnessMapResourceId;
    gpuData.aoId = definition->ambientOcclusionMapResourceId.isEmpty()
                       ? DEFAULT_WHITE_TEXTURE_ID
                       : definition->ambientOcclusionMapResourceId;
    gpuData.emissiveId = definition->emissiveMapResourceId.isEmpty()
                             ? DEFAULT_BLACK_TEXTURE_ID
                             : definition->emissiveMapResourceId;

    qInfo() << "Loading material" << materialId << "with textures:"
            << gpuData.albedoId << gpuData.normalId << gpuData.metallicRoughnessId << gpuData.aoId << gpuData.
            emissiveId;

    loadTexture(gpuData.albedoId);
    loadTexture(gpuData.normalId);
    loadTexture(gpuData.metallicRoughnessId);
    loadTexture(gpuData.aoId);
    loadTexture(gpuData.emissiveId);

    gpuData.ready = false;
    mMaterialCache.insert(materialId, std::move(gpuData));
}

RhiTextureGpuData *ResourceManager::getTextureGpuData(const QString &textureId) {
    return mTextureCache.contains(textureId) ? &mTextureCache[textureId] : nullptr;
}

bool ResourceManager::queueTextureUpdate(const QString &textureId, QRhiResourceUpdateBatch *batch) {
    if (!mRhi || !batch) {
        qWarning() << "Cannot queue texture update for" << textureId << "- RHI or batch invalid.";
        return false;
    }
    RhiTextureGpuData *gpuData = getTextureGpuData(textureId);
    if (!mTextureCache.contains(textureId)) {
        qWarning() << "Cannot queue texture update - Texture GPU data for ID" << textureId << "not found in cache.";
        return false;
    }
    gpuData = &mTextureCache[textureId];

    if (gpuData->ready) {
        return true;
    }
    if (!gpuData->texture) {
        qWarning() << "Cannot queue texture update - Texture RHI object missing for" << textureId;
        return false;
    }
    if (gpuData->sourceImage.isNull()) {
        qWarning() << "Cannot queue texture update - Source image is null for" << textureId <<
                ". Texture marked not ready.";
        if (textureId == DEFAULT_WHITE_TEXTURE_ID || textureId == DEFAULT_BLACK_TEXTURE_ID ||
            textureId == DEFAULT_NORMAL_MAP_ID || textureId == DEFAULT_METALROUGH_TEXTURE_ID) {
            qWarning() << "Attempting to reload default texture:" << textureId;
            loadAndQueueDefaultTextures(batch);
            gpuData = getTextureGpuData(textureId);
            if (gpuData && gpuData->ready) return true;
        }
        return false;
    }

    batch->uploadTexture(gpuData->texture.get(), gpuData->sourceImage);

    gpuData->sourceImage = QImage();
    gpuData->ready = true;
    return true;
}

RhiMaterialGpuData *ResourceManager::getMaterialGpuData(const QString &materialId) {
    return mMaterialCache.contains(materialId) ? &mMaterialCache[materialId] : nullptr;
}

bool ResourceManager::queueMaterialUpdate(const QString &materialId, QRhiResourceUpdateBatch *batch) {
    if (!mRhi || !batch) {
        qWarning() << "ResourceManager::queueMaterialUpdate - RHI or batch is null for" << materialId;
        return false;
    }

    RhiMaterialGpuData *gpuData = getMaterialGpuData(materialId);

    if (!gpuData) {
        qWarning() << "ResourceManager::queueMaterialUpdate - Material" << materialId << "not found in cache.";
        return false;
    }

    if (gpuData->ready) {
        return true;
    }

    qInfo() << "Queueing PBR material texture uploads for:" << materialId;

    bool allTexturesReadyOrQueued = true;

    auto queueTexture = [&](const QString &textureId, const QString &mapType) {
        if (textureId.isEmpty()) {
            qWarning() << "Material" << materialId << mapType << "texture ID is empty.";
            return;
        }
        RhiTextureGpuData *texGpu = getTextureGpuData(textureId);
        if (!texGpu) {
            qWarning() << "Material" << materialId << ": Referenced texture" << textureId <<
                    "not found in cache during update.";
            allTexturesReadyOrQueued = false;
            return;
        }
        if (!texGpu->ready) {
            if (queueTextureUpdate(textureId, batch)) {
                qInfo() << "  Queued update for" << mapType << "texture:" << textureId;
            } else {
                qWarning() << "  Failed to queue update for" << mapType << "texture:" << textureId;
                if (mapType == "Albedo" || mapType == "Normal" || mapType == "MetallicRoughness") {
                    allTexturesReadyOrQueued = false;
                }
            }
        }
    };

    queueTexture(gpuData->albedoId, "Albedo");
    queueTexture(gpuData->normalId, "Normal");
    queueTexture(gpuData->metallicRoughnessId, "MetallicRoughness");
    queueTexture(gpuData->aoId, "AO");
    queueTexture(gpuData->emissiveId, "Emissive");

    RhiTextureGpuData *albedoTexGpu = getTextureGpuData(gpuData->albedoId);
    if (allTexturesReadyOrQueued && albedoTexGpu && albedoTexGpu->ready) {
        qInfo() << "Material" << materialId << "marked as ready.";
        gpuData->ready = true;
    } else {
        qWarning() << "Material" << materialId << "could not be marked ready. All textures ready/queued:" <<
                allTexturesReadyOrQueued;
        gpuData->ready = false;
    }

    return gpuData->ready;
}

QString ResourceManager::generateMaterialCacheKey(const MaterialComponent *definition) {
    auto stripPrefix = [](const QString &id) {
        static const QString prefix = "builtin://textures/";
        return id.startsWith(prefix) ? id.mid(prefix.length()) : id;
    };

    return QString("mat_%1_%2_%3_%4_%5")
            .arg(stripPrefix(definition->albedoMapResourceId))
            .arg(stripPrefix(definition->normalMapResourceId))
            .arg(stripPrefix(definition->metallicRoughnessMapResourceId))
            .arg(stripPrefix(definition->ambientOcclusionMapResourceId))
            .arg(stripPrefix(definition->emissiveMapResourceId));
}

void ResourceManager::loadAndQueueDefaultTextures(QRhiResourceUpdateBatch *initialBatch) {
}
