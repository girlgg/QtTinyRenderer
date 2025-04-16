#pragma once

#include "ECSCore.h"
#include "RenderSystem.h"
#include "Component/MeshComponent.h"

class SceneManager;

const QVector<VertexData> DEFAULT_CUBE_VERTICES = {
    // Front face (Z+) - Normal (0, 0, 1)
    {{-0.5f, -0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}}, // Bottom Left
    {{0.5f, -0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f}}, // Bottom Right
    {{0.5f, 0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}}, // Top Right
    {{-0.5f, 0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}}, // Top Left
    // Back face (Z-) - Normal (0, 0, -1)
    {{-0.5f, -0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}, {1.0f, 0.0f}},
    {{-0.5f, 0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}, {1.0f, 1.0f}},
    {{0.5f, 0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}, {0.0f, 1.0f}},
    {{0.5f, -0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}, {0.0f, 0.0f}},
    // Left face (X-) - Normal (-1, 0, 0)
    {{-0.5f, -0.5f, -0.5f}, {-1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
    {{-0.5f, -0.5f, 0.5f}, {-1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}},
    {{-0.5f, 0.5f, 0.5f}, {-1.0f, 0.0f, 0.0f}, {1.0f, 1.0f}},
    {{-0.5f, 0.5f, -0.5f}, {-1.0f, 0.0f, 0.0f}, {0.0f, 1.0f}},
    // Right face (X+) - Normal (1, 0, 0)
    {{0.5f, -0.5f, 0.5f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
    {{0.5f, -0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}},
    {{0.5f, 0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}, {1.0f, 1.0f}},
    {{0.5f, 0.5f, 0.5f}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f}},
    // Top face (Y+) - Normal (0, 1, 0)
    {{-0.5f, 0.5f, 0.5f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}},
    {{0.5f, 0.5f, 0.5f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}},
    {{0.5f, 0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f}},
    {{-0.5f, 0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f}},
    // Bottom face (Y-) - Normal (0, -1, 0)
    {{-0.5f, -0.5f, -0.5f}, {0.0f, -1.0f, 0.0f}, {0.0f, 0.0f}},
    {{0.5f, -0.5f, -0.5f}, {0.0f, -1.0f, 0.0f}, {1.0f, 0.0f}},
    {{0.5f, -0.5f, 0.5f}, {0.0f, -1.0f, 0.0f}, {1.0f, 1.0f}},
    {{-0.5f, -0.5f, 0.5f}, {0.0f, -1.0f, 0.0f}, {0.0f, 1.0f}},
};
const QVector<quint16> DEFAULT_CUBE_INDICES = {
    0, 1, 2, 2, 3, 0, // Front
    4, 5, 6, 6, 7, 4, // Back
    8, 9, 10, 10, 11, 8, // Left
    12, 13, 14, 14, 15, 12, // Right
    16, 17, 18, 18, 19, 16, // Top
    20, 21, 22, 22, 23, 20 // Bottom
};

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
    QScopedPointer<QRhiShaderResourceBindings> mMaterialBindings; // Template for material texture/sampler
    QScopedPointer<QRhiBuffer> mInstanceUbo; // Dynamic buffer for model matrices
    QScopedPointer<QRhiSampler> mDefaultSampler;
    QVector<InstanceUniformBlock> mInstanceDataBuffer; // CPU-side buffer for instance data
    int mMaxInstances = 256; // Example limit for dynamic UBO
    qint32 mInstanceBlockAlignedSize = 0;

    EntityID mActiveCamera = INVALID_ENTITY;

    // --- Caches ---
    // Set 1 (Material): Texture ID -> SRB
    QHash<QString, QSharedPointer<QRhiShaderResourceBindings> > mMaterialBindingsCache;
    // Set 2 (Instance): Offset -> SRB
    QHash<quint32, QSharedPointer<QRhiShaderResourceBindings> > mInstanceBindingsCache;

    const QString BUILTIN_CUBE_MESH_ID = "builtin_cube"; // ID for the default cube mesh
};
