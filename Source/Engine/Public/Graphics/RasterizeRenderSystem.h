#pragma once

#include "ECSCore.h"
#include "RenderSystem.h"
#include "Component/MeshComponent.h"

class SceneManager;

class RasterizeRenderSystem final : public RenderSystem {
public:
    RasterizeRenderSystem();

    ~RasterizeRenderSystem() override;

    void initialize(QRhi *rhi, QRhiSwapChain *swapChain, QRhiRenderPassDescriptor *rp, QSharedPointer<World> world,
                    QSharedPointer<ResourceManager> resManager) override;

    void releaseResources();

    void releasePipelines();

    void releaseInstanceResources();

    void releaseGlobalResources();

    void resize(QSize size) override;

    void draw(QRhiCommandBuffer *cmdBuffer, const QSize &outputSizeInPixels);

    void submitResourceUpdates(QRhiResourceUpdateBatch *batch) override;

protected:
    void createGlobalResources();

    void createInstanceResources();

    void createPipelines();

    void findActiveCamera();

    QScopedPointer<QRhiBuffer> mCameraUbo;
    QScopedPointer<QRhiBuffer> mLightingUbo;
    QScopedPointer<QRhiShaderResourceBindings> mGlobalBindings;
    QSharedPointer<QRhiGraphicsPipeline> mBasePipeline;
    QSharedPointer<QRhiShaderResourceBindings> mPipelineSrbLayoutDef;
    QScopedPointer<QRhiShaderResourceBindings> mMaterialBindings;
    QScopedPointer<QRhiBuffer> mInstanceUbo;
    QScopedPointer<QRhiSampler> mDefaultSampler;
    QVector<InstanceUniformBlock> mInstanceDataBuffer;
    int mMaxInstances = 256;
    qint32 mInstanceBlockAlignedSize = 0;

    EntityID mActiveCamera = INVALID_ENTITY;

    // --- Caches ---
    // Texture ID -> SRB
    QHash<QString, QSharedPointer<QRhiShaderResourceBindings> > mMaterialBindingsCache;
    // Offset -> SRB
    QHash<quint32, QSharedPointer<QRhiShaderResourceBindings> > mInstanceBindingsCache;
};
