#pragma once
#include <rhi/qrhi.h>

class World;
class SceneManager;
class ResourceManager;

class RenderSystem {
public:
    virtual ~RenderSystem() = default;

    virtual void initialize(QRhi *rhi, QRhiSwapChain *swapChain, QRhiRenderPassDescriptor *rp,
                            QSharedPointer<World> world,
                            QSharedPointer<ResourceManager> resManager) = 0;

    virtual void resize(QSize size) = 0;

    virtual void submitResourceUpdates(QRhiResourceUpdateBatch *batch) = 0;

protected:
    QRhi *mRhi = nullptr;
    QRhiSwapChain *mSwapChain = nullptr;
    QRhiRenderPassDescriptor *mSwapChainPassDesc = nullptr;
    QSharedPointer<World> mWorld = nullptr;
    QSharedPointer<ResourceManager> mResourceManager = nullptr;
    QSize mOutputSize;
};
