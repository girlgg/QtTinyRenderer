#pragma once
#include <rhi/qrhi.h>

class World;
class SceneManager;
class ResourceManager;

// Define UBO structures (matching shader expectations)
struct CameraUniformBlock {
    QGenericMatrix<4, 4, float> view;
    QGenericMatrix<4, 4, float> projection;
    QVector3D viewPos;
    // Add padding if needed for alignment rules (std140 etc.)
    float padding;
};

struct LightingUniformBlock {
    // Simplified: One directional light + arrays for point/spot
    // Matching shader structure is crucial
    QVector3D dirLightDirection;
    float padding1; // Example padding
    QVector3D dirLightAmbient;
    float padding2;
    QVector3D dirLightDiffuse;
    float padding3;
    QVector3D dirLightSpecular;
    float padding4;

    // Add arrays for point lights, spot lights etc.
    // int numPointLights; etc.
};

// Per-instance data (Model matrix) - Can be UBO or Push Constants
struct InstanceUniformBlock {
    QGenericMatrix<4, 4, float> model;
    // Add Normal Matrix etc. if needed
};

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
