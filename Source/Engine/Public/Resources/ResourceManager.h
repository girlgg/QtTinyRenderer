#pragma once

#include <QHash>
#include <QString>

#include "ShaderBundle.h"

struct VertexData;
class QRhi;

struct RhiMeshGpuData {
    QSharedPointer<QRhiBuffer> vertexBuffer;
    QSharedPointer<QRhiBuffer> indexBuffer;
    qint32 indexCount = 0;
    bool ready = false;
};

struct RhiMaterialGpuData {
    QSharedPointer<QRhiTexture> texture;
    QSharedPointer<QRhiSampler> sampler;
    bool ready = false;
    QImage sourceImage;
};

class ResourceManager {
public:
    void initialize(QSharedPointer<QRhi> rhi);

    void releaseRhiResources();

    void loadMeshFromData(const QString &id, const QVector<VertexData> &vertices, const QVector<quint16> &indices);

    RhiMeshGpuData *getMeshGpuData(const QString &id);

    bool queueMeshUpdate(const QString &id, QRhiResourceUpdateBatch *batch, const QVector<VertexData> &vertices,
                         const QVector<quint16> &indices);

    void loadMaterialTexture(const QString &textureId);

    RhiMaterialGpuData *getMaterialGpuData(const QString &textureId);

    bool queueMaterialUpdate(const QString &textureId, QRhiResourceUpdateBatch *batch);

private:
    QSharedPointer<QRhi> mRhi;
    QHash<QString, RhiMeshGpuData> mMeshCache;
    QHash<QString, RhiMaterialGpuData> mMaterialCache;
};
