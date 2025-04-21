#include "Resources/ResourceManager.h"

#include "Component/MaterialComponent.h"
#include "Component/MeshComponent.h"

void ResourceManager::initialize(QSharedPointer<QRhi> rhi) {
    mRhi = rhi;
    if (mRhi) {
        loadDefaultResources();
    } else {
        qWarning() << "ResourceManager initialized with null QRhi pointer!";
    }
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
    // --- 矫正size ---
    gpuData.vertexBuffer.reset(mRhi->newBuffer(QRhiBuffer::Immutable,
                                               QRhiBuffer::VertexBuffer,
                                               vertices.size() * sizeof(VertexData)));
    if (!gpuData.vertexBuffer || !gpuData.vertexBuffer->create()) {
        qWarning() << "Failed to create vertex buffer for" << id;
        return;
    }

    gpuData.indexBuffer.reset(mRhi->newBuffer(QRhiBuffer::Immutable, QRhiBuffer::IndexBuffer,
                                              indices.size() * sizeof(quint16)));
    if (!gpuData.indexBuffer || !gpuData.indexBuffer->create()) {
        qWarning() << "Failed to create index buffer for" << id;
        gpuData.vertexBuffer.reset();
        return;
    }

    gpuData.indexCount = indices.size();
    gpuData.ready = false;

    mMeshCache.insert(id, std::move(gpuData));
}

RhiMeshGpuData *ResourceManager::getMeshGpuData(const QString &id) {
    if (mMeshCache.count(id))
        return &mMeshCache[id];
    return nullptr;
}

bool ResourceManager::queueMeshUpdate(const QString &id, QRhiResourceUpdateBatch *batch,
                                      const QVector<VertexData> &vertices, const QVector<quint16> &indices) {
    if (!mRhi || !batch) return false;
    RhiMeshGpuData *gpuData = getMeshGpuData(id);
    if (!gpuData || gpuData->ready) {
        return true;
    }

    batch->uploadStaticBuffer(gpuData->vertexBuffer.get(), 0, vertices.size() * sizeof(VertexData),
                              vertices.constData());
    batch->uploadStaticBuffer(gpuData->indexBuffer.get(), 0, indices.size() * sizeof(quint16), indices.constData());

    gpuData->ready = true;
    return true;
}

void ResourceManager::loadTexture(const QString &textureId) {
    if (mMaterialCache.contains(textureId) || !mRhi) return;

    QImage image(textureId);
    if (image.isNull()) {
        qWarning() << "Failed to load texture:" << textureId;
        return;
    }
    image = image.convertToFormat(QImage::Format_RGBA8888);

    RhiTextureGpuData texGpuData;
    texGpuData.texture.reset(mRhi->newTexture(QRhiTexture::RGBA8, image.size(), 1));
    if (!texGpuData.texture || !texGpuData.texture->create()) {
        qWarning() << "Failed to create texture for" << textureId;
        return;
    }
    texGpuData.sourceImage = image;
    texGpuData.ready = false;
    mTextureCache.insert(textureId, std::move(texGpuData));
}

void ResourceManager::loadMaterial(const QString &materialId,const MaterialComponent *definition) {
    if (mMaterialCache.contains(materialId) || !mRhi) return;

    RhiMaterialGpuData gpuData;

    gpuData.albedoId = definition->albedoMapResourceId;
    gpuData.normalId = definition->normalMapResourceId;
    gpuData.metallicRoughnessId = definition->metallicRoughnessMapResourceId;
    gpuData.aoId = definition->ambientOcclusionMapResourceId;
    gpuData.emissiveId = definition->emissiveMapResourceId;

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

RhiMaterialGpuData *ResourceManager::getMaterialGpuData(const QString &materialId) {
    return mMaterialCache.contains(materialId) ? &mMaterialCache[materialId] : nullptr;
}

bool ResourceManager::queueMaterialUpdate(const QString &materialId, QRhiResourceUpdateBatch *batch) {
    if (!mRhi || !batch) return false;

    RhiMaterialGpuData *gpuData = getMaterialGpuData(materialId);

    if (!gpuData) {
        return false;
    }
    if (gpuData->ready)
        return true;

    qInfo() << "Queueing PBR material texture uploads for:" << materialId;

    bool success = true;

    auto loadTextureById = [&](const QString &textureId) {
        if (!textureId.isEmpty() && mTextureCache.contains(textureId)) {
            RhiTextureGpuData &tex = mTextureCache[textureId];
            batch->uploadTexture(tex.texture.get(), tex.sourceImage);
            tex.sourceImage = QImage();
        } else if (textureId.isEmpty()) {
            success = false;
        }
    };

    loadTextureById(gpuData->albedoId);
    loadTextureById(gpuData->normalId);
    loadTextureById(gpuData->metallicRoughnessId);
    loadTextureById(gpuData->aoId);
    loadTextureById(gpuData->emissiveId);

    if (success) {
        gpuData->ready = true;
        qInfo() << "Material ready:" << materialId;
    } else {
        qWarning() << "Failed to queue update for all textures in material:" << materialId;
    }

    return success;
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
