#pragma once

#include <QHash>
#include <QString>

#include "CommonRender.h"
#include "ShaderBundle.h"

struct MaterialComponent;
struct VertexData;
class QRhi;

struct RhiMeshGpuData {
    QSharedPointer<QRhiBuffer> vertexBuffer;
    QSharedPointer<QRhiBuffer> indexBuffer;
    qint32 indexCount = 0;
    qint32 vertexCount = 0;
    bool ready = false;
};

struct RhiTextureGpuData {
    QSharedPointer<QRhiTexture> texture;
    QImage sourceImage;
    bool ready = false;
};

struct RhiMaterialGpuData {
    QString albedoId = DEFAULT_WHITE_TEXTURE_ID;
    QString normalId = DEFAULT_NORMAL_MAP_ID;
    QString metallicRoughnessId = DEFAULT_METALROUGH_TEXTURE_ID;
    QString aoId = DEFAULT_WHITE_TEXTURE_ID;
    QString emissiveId = DEFAULT_BLACK_TEXTURE_ID;

    bool ready = false;
};

class ResourceManager {
public:
    ~ResourceManager();

    void initialize(QSharedPointer<QRhi> rhi);

    void loadDefaultResources();

    void releaseRhiResources();

    // --- Mesh Management ---

    void loadMeshFromData(const QString &id, const QVector<VertexData> &vertices, const QVector<quint16> &indices);

    RhiMeshGpuData *getMeshGpuData(const QString &id);

    bool queueMeshUpdate(const QString &id, QRhiResourceUpdateBatch *batch, const QVector<VertexData> &vertices,
                         const QVector<quint16> &indices);

    // --- Texture Management ---

    void loadTexture(const QString &textureId);

    RhiTextureGpuData *getTextureGpuData(const QString &textureId);

    bool queueTextureUpdate(const QString &textureId, QRhiResourceUpdateBatch *batch);

    // --- Material Management ---

    void loadMaterial(const QString &materialId, const MaterialComponent *definition);

    RhiMaterialGpuData *getMaterialGpuData(const QString &materialId);

    bool queueMaterialUpdate(const QString &textureId, QRhiResourceUpdateBatch *batch);

    // --- Helpers ---

    QString generateMaterialCacheKey(const MaterialComponent *definition);

private:
    void loadAndQueueDefaultTextures(QRhiResourceUpdateBatch *initialBatch);

    void createDefaultTextures();

    QSharedPointer<QRhi> mRhi;

    QHash<QString, RhiMeshGpuData> mMeshCache;
    QHash<QString, RhiTextureGpuData> mTextureCache;
    QHash<QString, RhiMaterialGpuData> mMaterialCache;
};
