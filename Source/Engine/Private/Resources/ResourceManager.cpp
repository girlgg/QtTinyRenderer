#include "Resources/ResourceManager.h"

#include "Component/MeshComponent.h"

void ResourceManager::initialize(QSharedPointer<QRhi> rhi) {
    mRhi = rhi;
    if (!mRhi) {
        qWarning() << "ResourceManager initialized with null QRhi pointer!";
    }
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

void ResourceManager::loadMaterialTexture(const QString &textureId) {
    if (mMaterialCache.contains(textureId) || !mRhi) return;

    QImage image(textureId);
    if (image.isNull()) {
        qWarning() << "Failed to load texture:" << textureId;
        return;
    }
    image = image.convertToFormat(QImage::Format_RGBA8888);

    RhiMaterialGpuData gpuData;
    gpuData.texture.reset(mRhi->newTexture(QRhiTexture::RGBA8, image.size(), 1));
    if (!gpuData.texture || !gpuData.texture->create()) {
        qWarning() << "Failed to create texture for" << textureId;
        return;
    }

    gpuData.sourceImage = image;
    gpuData.ready = false;
    mMaterialCache.insert(textureId, std::move(gpuData));
}

RhiMaterialGpuData *ResourceManager::getMaterialGpuData(const QString &textureId) {
    if (mMaterialCache.count(textureId)) return &mMaterialCache[textureId];
    return nullptr;
}

bool ResourceManager::queueMaterialUpdate(const QString &textureId, QRhiResourceUpdateBatch *batch) {
    if (!mRhi) return false;
    RhiMaterialGpuData *gpuData = getMaterialGpuData(textureId);
    if (!gpuData || gpuData->ready)
        return true;

    batch->uploadTexture(gpuData->texture.get(), gpuData->sourceImage);
    gpuData->ready = true;
    gpuData->sourceImage = QImage();
    return true;
}
