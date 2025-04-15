#include "Graphics/RasterizeRenderSystem.h"

#include "Component/LightComponent.h"
#include "Component/MaterialComponent.h"
#include "Component/RenderableComponent.h"
#include "Resources/ResourceManager.h"
#include "Scene/SceneManager.h"
#include "Scene/World.h"

RasterizeRenderSystem::RasterizeRenderSystem() {
}

RasterizeRenderSystem::~RasterizeRenderSystem() {
    releaseResources();
}

void RasterizeRenderSystem::initialize(QRhi *rhi, QRhiSwapChain *swapChain, QRhiRenderPassDescriptor *rp,
                                       QSharedPointer<World> world,
                                       QSharedPointer<ResourceManager> resManager) {
    mRhi = rhi;
    mSwapChain = swapChain;
    mSwapChainPassDesc = rp;
    mWorld = world;
    mResourceManager = resManager;

    if (!mRhi || !mSwapChain || !mSwapChainPassDesc || !mWorld || !mResourceManager) {
        qWarning() << "RasterizeRenderSystem::initialize - Invalid arguments provided.";
        return;
    }

    mOutputSize = swapChain->currentPixelSize();

    qInfo() << "Initializing RasterizeRenderSystem...";

    quint32 alignment = mRhi->ubufAlignment();
    mInstanceBlockAlignedSize = sizeof(InstanceUniformBlock);
    if (mInstanceBlockAlignedSize % alignment != 0) {
        mInstanceBlockAlignedSize += alignment - (mInstanceBlockAlignedSize % alignment);
    }
    qInfo() << "Instance UBO block size (raw):" << sizeof(InstanceUniformBlock)
            << "Aligned size:" << mInstanceBlockAlignedSize
            << "Alignment requirement:" << alignment;

    createGlobalResources(); // Set 0: Camera/Light UBOs, Sampler, Global SRB
    createInstanceResources(); // Set 2: Instance UBO + CPU buffer
    createPipelines(); // Defines pipeline expecting Sets 0, 1, 2

    findActiveCamera();

    if (!mResourceManager->getMeshGpuData(BUILTIN_CUBE_MESH_ID)) {
        qInfo() << "Loading built-in cube mesh data...";
        mResourceManager->loadMeshFromData(BUILTIN_CUBE_MESH_ID,
                                           DEFAULT_CUBE_VERTICES,
                                           DEFAULT_CUBE_INDICES);
        // Mark it as needing upload
        RhiMeshGpuData *meshGpu = mResourceManager->getMeshGpuData(BUILTIN_CUBE_MESH_ID);
        if (meshGpu) meshGpu->ready = false;
    }

    qInfo() << "RasterizeRenderSystem initialized successfully.";
}

void RasterizeRenderSystem::releaseResources() {
    qInfo() << "Releasing RasterizeRenderSystem resources...";
    // Release in reverse order of creation potentially
    // releasePipelines();
    // releaseInstanceResources();
    // releaseGlobalResources();

    // Clear caches
    mMaterialBindingsCache.clear();
    mInstanceBindingsCache.clear();

    // Invalidate pointers (important if this object outlives the RHI context somehow)
    mRhi = nullptr;
    mSwapChain = nullptr;
    mSwapChainPassDesc = nullptr;
    mWorld.reset();
    mResourceManager.reset();
}

void RasterizeRenderSystem::releaseInstanceResources() {
    mInstanceUbo.reset();
    mInstanceDataBuffer.clear(); // Release CPU memory
}

void RasterizeRenderSystem::resize(QSize size) {
    if (size.isValid()) {
        mOutputSize = size;
        qInfo() << "RasterizeRenderSystem resized to" << size;
        // Note: Camera aspect ratio update is handled externally (e.g., in ViewWindow)
        // by getting the camera component and setting its aspect ratio.
    }
}

void RasterizeRenderSystem::draw(QRhiCommandBuffer *cmdBuffer, const QSize &outputSizeInPixels) {
    if (!mWorld || !mResourceManager || !mBasePipeline || !mGlobalBindings || !cmdBuffer || !mInstanceUbo || !mRhi) {
        qWarning() << "draw - Preconditions not met.";
        return;
    }

    // Get the cube mesh (should be ready after submitResourceUpdates)
    RhiMeshGpuData *cubeMeshGpu = mResourceManager->getMeshGpuData(BUILTIN_CUBE_MESH_ID);
    if (!cubeMeshGpu || !cubeMeshGpu->ready) {
        qWarning() << "Cube mesh data not ready for drawing.";
        return; // Cannot draw
    }

    // --- Setup Common State ---
    cmdBuffer->setGraphicsPipeline(mBasePipeline.get());
    cmdBuffer->setViewport({0, 0, (float) mOutputSize.width(), (float) mOutputSize.height()});

    // Bind Set 0 SRB (Global UBOs) - Once
    // cmdBuffer->setShaderResources(mGlobalBindings.get());

    // Bind Cube Vertex/Index Buffers - Once
    QRhiCommandBuffer::VertexInput vtxBinding(cubeMeshGpu->vertexBuffer.get(), 0);
    cmdBuffer->setVertexInput(0, 1, &vtxBinding, cubeMeshGpu->indexBuffer.get(), 0, QRhiCommandBuffer::IndexUInt16);

    // --- Draw Loop ---
    int currentInstanceIndex = 0; // Keep track of which instance slot we are using

    // Group entities by material for efficiency
    QHash<QString, QVector<EntityID> > entitiesByMaterial;
    for (EntityID entity: mWorld->view<RenderableComponent, MaterialComponent, TransformComponent>()) {
        const auto *renderable = mWorld->getComponent<RenderableComponent>(entity);
        const auto *matComp = mWorld->getComponent<MaterialComponent>(entity);
        const auto *tfComp = mWorld->getComponent<TransformComponent>(entity); // Need transform to exist
        if (renderable && renderable->isVisible && matComp && tfComp) {
            entitiesByMaterial[matComp->textureResourceId].append(entity);
        }
    }

    // Iterate through materials
    for (auto it = entitiesByMaterial.constBegin(); it != entitiesByMaterial.constEnd(); ++it) {
        const QString &textureId = it.key();
        const QVector<EntityID> &entities = it.value();

        // Get Material GPU Data (Texture)
        RhiMaterialGpuData *matGpu = mResourceManager->getMaterialGpuData(textureId);
        if (!matGpu || !matGpu->ready || !matGpu->texture || !mDefaultSampler) {
            qWarning() << "Material resources not ready for texture:" << textureId << "Skipping" << entities.size() <<
                    "entities.";
            currentInstanceIndex += entities.size(); // Advance index even if skipped
            continue; // Skip this material group
        }

        // Draw all entities using this material
        for (EntityID entity: entities) {
            if (currentInstanceIndex >= mMaxInstances) break;

            quint32 dynamicOffset = currentInstanceIndex * mInstanceBlockAlignedSize;

            // *** Create ONE SRB containing ALL bindings for THIS draw ***
            QScopedPointer drawSrb(mRhi->newShaderResourceBindings());
            drawSrb->setName(QString("DrawSRB_Inst%1_Mat%2").arg(currentInstanceIndex).arg(textureId).toUtf8());
            drawSrb->setBindings({
                // Binding 0: Camera UBO (Global)
                QRhiShaderResourceBinding::uniformBuffer(0,
                                                         QRhiShaderResourceBinding::VertexStage |
                                                         QRhiShaderResourceBinding::FragmentStage,
                                                         mCameraUbo.get()), // Use actual UBO
                // Binding 1: Light UBO (Global)
                QRhiShaderResourceBinding::uniformBuffer(1,
                                                         QRhiShaderResourceBinding::FragmentStage,
                                                         mLightingUbo.get()), // Use actual UBO
                // Binding 2: Texture Sampler (Material Specific)
                QRhiShaderResourceBinding::sampledTexture(2,
                                                          QRhiShaderResourceBinding::FragmentStage,
                                                          matGpu->texture.get(), mDefaultSampler.get()),
                // Use material's texture
                // Binding 3: Instance UBO Slice (Instance Specific)
                QRhiShaderResourceBinding::uniformBuffer(3,
                                                         QRhiShaderResourceBinding::VertexStage,
                                                         mInstanceUbo.get(), dynamicOffset,
                                                         mInstanceBlockAlignedSize) // Use instance UBO slice
            });

            if (!drawSrb->create()) {
                qWarning() << "Failed to create per-draw SRB for instance" << currentInstanceIndex;
                // Don't advance index if create failed? Or depends on why it failed.
                // Let's advance for now to avoid infinite loop on persistent error.
                currentInstanceIndex++;
                continue;
            }

            // *** Bind the single, complete SRB for this draw ***
            cmdBuffer->setShaderResources(drawSrb.get());

            // --- Issue Draw Call ---
            cmdBuffer->drawIndexed(cubeMeshGpu->indexCount);

            currentInstanceIndex++;
        } // End entity loop
        if (currentInstanceIndex >= mMaxInstances) break;
    } // End loop for materials
}

void RasterizeRenderSystem::renderFrame(QRhiCommandBuffer *cmdBuffer) {
}

void RasterizeRenderSystem::submitResourceUpdates(QRhiResourceUpdateBatch *batch) {
    if (!mWorld || !mResourceManager || !batch || !mRhi || !mCameraUbo || !mLightingUbo || !mInstanceUbo) {
        qWarning() << "submitResourceUpdates - Preconditions not met.";
        return;
    }

    CameraUniformBlock camData;
    bool cameraOk = false;
    if (mActiveCamera != INVALID_ENTITY) {
        auto *camComp = mWorld->getComponent<CameraComponent>(mActiveCamera);
        auto *camTf = mWorld->getComponent<TransformComponent>(mActiveCamera);
        if (camComp && camTf) {
            QMatrix4x4 viewMatrix;
            // Ensure TransformComponent methods are correct (e.g., position(), getRotation())
            // Using rotatedVector assumes Camera looks down -Z initially
            viewMatrix.lookAt(camTf->position(), camTf->position() + camTf->rotation().rotatedVector({0, 0, -1}),
                              camTf->rotation().rotatedVector({0, 1, 0}));

            QMatrix4x4 projMatrix;
            // Assuming CameraComponent provides these parameters
            projMatrix.perspective(camComp->mFov, camComp->mAspect, camComp->mNearPlane, camComp->mFarPlane);
            projMatrix *= mRhi->clipSpaceCorrMatrix(); // Apply Vulkan correction

            camData.view = viewMatrix.toGenericMatrix<4, 4>();
            camData.projection = projMatrix.toGenericMatrix<4, 4>();
            camData.viewPos = camTf->position();
            cameraOk = true;
        } else {
            qWarning() << "Active camera entity" << mActiveCamera << "missing required components.";
        }
    } else {
        // Handle case where there's no camera (identity matrices?)
        findActiveCamera(); // Try to find one again
    }
    if (!cameraOk) {
        findActiveCamera(); // Try to find next frame
        // Set default values?
        QMatrix4x4 identity;
        camData.view = identity.toGenericMatrix<4, 4>();
        camData.projection = identity.toGenericMatrix<4, 4>();
        camData.viewPos = QVector3D(0, 0, 0);
    }
    batch->updateDynamicBuffer(mCameraUbo.get(), 0, sizeof(CameraUniformBlock), &camData);

    // --- Update Lighting UBO (Set 0, Binding 1) ---
    LightingUniformBlock lightData;
    bool lightFound = false;
    // Find first directional light
    for (EntityID entity: mWorld->view<LightComponent, TransformComponent>()) {
        const auto *lightComp = mWorld->getComponent<LightComponent>(entity);
        const auto *lightTf = mWorld->getComponent<TransformComponent>(entity);
        if (lightComp && lightTf && lightComp->type == LightType::Directional) {
            lightData.dirLightDirection = lightTf->rotation().rotatedVector({0, 0, -1}).normalized();
            // Use transform rotation
            lightData.dirLightAmbient = lightComp->ambient;
            lightData.dirLightDiffuse = lightComp->diffuse;
            lightData.dirLightSpecular = lightComp->specular;
            lightFound = true;
            break;
        }
    }
    if (!lightFound) {
        // Default values if no light
        lightData.dirLightDirection = QVector3D(0, -1, 0);
        lightData.dirLightAmbient = QVector3D(0.1f, 0.1f, 0.1f);
        lightData.dirLightDiffuse = QVector3D(0.0f, 0.0f, 0.0f);
        lightData.dirLightSpecular = QVector3D(0.0f, 0.0f, 0.0f);
    }
    batch->updateDynamicBuffer(mLightingUbo.get(), 0, sizeof(LightingUniformBlock), &lightData);

    // --- Ensure Shared Resources are Uploaded ---
    // Built-in Cube Mesh
    RhiMeshGpuData *cubeMeshGpu = mResourceManager->getMeshGpuData(BUILTIN_CUBE_MESH_ID);
    if (!cubeMeshGpu) {
        // Should have been loaded in initialize, but check again
        mResourceManager->loadMeshFromData(BUILTIN_CUBE_MESH_ID, DEFAULT_CUBE_VERTICES, DEFAULT_CUBE_INDICES);
        cubeMeshGpu = mResourceManager->getMeshGpuData(BUILTIN_CUBE_MESH_ID);
        if (cubeMeshGpu) cubeMeshGpu->ready = false; // Mark for upload
        else
            qWarning() << "Failed to load cube mesh data even on demand.";
    }
    if (cubeMeshGpu && !cubeMeshGpu->ready) {
        mResourceManager->queueMeshUpdate(BUILTIN_CUBE_MESH_ID, batch, DEFAULT_CUBE_VERTICES, DEFAULT_CUBE_INDICES);
    }

    // Textures for visible materials
    QSet<QString> uniqueTextureIds;
    for (EntityID entity: mWorld->view<RenderableComponent, MaterialComponent>()) {
        const auto *renderable = mWorld->getComponent<RenderableComponent>(entity);
        const auto *matComp = mWorld->getComponent<MaterialComponent>(entity);
        if (renderable && renderable->isVisible && matComp) {
            uniqueTextureIds.insert(matComp->textureResourceId);
        }
    }
    for (const QString &textureId: qAsConst(uniqueTextureIds)) {
        if (!mResourceManager->getMaterialGpuData(textureId)) {
            mResourceManager->loadMaterialTexture(textureId);
        }
        RhiMaterialGpuData *matGpu = mResourceManager->getMaterialGpuData(textureId);
        // Check pointer and readiness
        if (matGpu && !matGpu->ready) {
            mResourceManager->queueMaterialUpdate(textureId, batch);
        } else if (!matGpu) {
            qWarning() << "Material GPU data still null after load attempt for:" << textureId;
        }
    }


    // --- Collect Instance Data and Update Instance UBO (Set 2) ---
    int currentInstanceIndex = 0;
    // Iterate entities that have all necessary components for drawing
    for (EntityID entity: mWorld->view<RenderableComponent, TransformComponent, MaterialComponent>()) {
        if (currentInstanceIndex >= mMaxInstances) {
            qWarning() << "Exceeded max instances per batch:" << mMaxInstances;
            break;
        }
        const auto *renderable = mWorld->getComponent<RenderableComponent>(entity);
        const auto *tfComp = mWorld->getComponent<TransformComponent>(entity);
        // MaterialComponent needed for grouping later, TransformComponent needed for matrix

        if (renderable && renderable->isVisible && tfComp) {
            // Assuming TransformComponent provides world matrix correctly
            mInstanceDataBuffer[currentInstanceIndex].model = tfComp->worldMatrix().toGenericMatrix<4, 4>();

            currentInstanceIndex++;
        }
    }

    // Upload the collected data
    if (currentInstanceIndex > 0) {
        // *** Use aligned size for total upload size ***
        batch->updateDynamicBuffer(mInstanceUbo.get(), 0,
                                   currentInstanceIndex * mInstanceBlockAlignedSize,
                                   mInstanceDataBuffer.data());
    }
}

void RasterizeRenderSystem::createGlobalResources() {
    // --- UBOs ---
    mCameraUbo.reset(mRhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, sizeof(CameraUniformBlock)));
    if (!mCameraUbo->create())
        qFatal("Failed to create camera UBO");
    mCameraUbo->setName(QByteArrayLiteral("Camera UBO"));

    mLightingUbo.reset(mRhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, sizeof(LightingUniformBlock)));
    if (!mLightingUbo->create())
        qFatal("Failed to create lighting UBO");
    mLightingUbo->setName(QByteArrayLiteral("Lighting UBO"));

    // --- Default Sampler ---
    mDefaultSampler.reset(mRhi->newSampler(QRhiSampler::Linear,
                                           QRhiSampler::Linear,
                                           QRhiSampler::Linear,
                                           QRhiSampler::ClampToEdge,
                                           QRhiSampler::ClampToEdge));
    if (!mDefaultSampler->create())
        qFatal("Failed to create default sampler");
    mDefaultSampler->setName(QByteArrayLiteral("Default Sampler"));

    // Create the SRB instance for Set 0
    mGlobalBindings.reset(mRhi->newShaderResourceBindings());
    mGlobalBindings->setBindings({
        QRhiShaderResourceBinding::uniformBuffer(0,
                                                 QRhiShaderResourceBinding::VertexStage |
                                                 QRhiShaderResourceBinding::FragmentStage,
                                                 mCameraUbo.get()),
        QRhiShaderResourceBinding::uniformBuffer(1,
                                                 QRhiShaderResourceBinding::FragmentStage,
                                                 mLightingUbo.get())
    });
    if (!mGlobalBindings->create())
        qFatal("Failed to create global SRB");
    mGlobalBindings->setName(QByteArrayLiteral("Global SRB"));
}

void RasterizeRenderSystem::createInstanceResources() {
    // Create the large dynamic UBO for instance data
    mInstanceUbo.reset(mRhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer,
                                       mMaxInstances * mInstanceBlockAlignedSize)); // Use aligned size
    if (!mInstanceUbo || !mInstanceUbo->create())
        qFatal("Failed to create instance UBO");
    mInstanceUbo->setName(QByteArrayLiteral("Instance Data UBO"));

    // Pre-allocate CPU buffer
    mInstanceDataBuffer.resize(mMaxInstances);
    qInfo() << "Instance UBO created with capacity for" << mMaxInstances << "instances.";
}

void RasterizeRenderSystem::createPipelines() {
    const quint32 vertexStride = sizeof(VertexData);
    QRhiVertexInputLayout inputLayout;
    inputLayout.setBindings({QRhiVertexInputBinding(vertexStride)});
    inputLayout.setAttributes({
        QRhiVertexInputAttribute(0, 0, QRhiVertexInputAttribute::Float3, 0), // Pos at offset 0
        QRhiVertexInputAttribute(0, 1, QRhiVertexInputAttribute::Float3, sizeof(QVector3D)), // Norm after Pos
        QRhiVertexInputAttribute(0, 2, QRhiVertexInputAttribute::Float2, sizeof(QVector3D) * 2) // UV after Pos and Norm
    });
    ShaderBundle::getInstance()->loadShader("Shaders/baseGeo", {
                                                {":/shaders/baseGeo.vert.qsb", QRhiShaderStage::Vertex},
                                                {":/shaders/baseGeo.frag.qsb", QRhiShaderStage::Fragment}
                                            });
    QRhiShaderStage vs = ShaderBundle::getInstance()->getShaderStage("Shaders/baseGeo", QRhiShaderStage::Vertex);
    QRhiShaderStage fs = ShaderBundle::getInstance()->getShaderStage("Shaders/baseGeo", QRhiShaderStage::Fragment);

    // --- Define Pipeline ---
    mBasePipeline.reset(mRhi->newGraphicsPipeline());
    mBasePipeline->setName(QByteArrayLiteral("Base Opaque Pipeline"));

    // --- Define the *SINGLE, COMBINED* SRB Layout for the Pipeline ---
    // This SRB lists *all* expected bindings across *all* sets (0, 1, 2).
    // Use nullptr for resource pointers as this is just layout definition.

    mPipelineSrbLayoutDef.reset(mRhi->newShaderResourceBindings());
    mPipelineSrbLayoutDef->setName(QByteArrayLiteral("Pipeline Combined Layout Def"));
    mPipelineSrbLayoutDef->setBindings({
        QRhiShaderResourceBinding::uniformBuffer(0,
                                                 QRhiShaderResourceBinding::VertexStage |
                                                 QRhiShaderResourceBinding::FragmentStage,
                                                 nullptr), // Camera UBO
        QRhiShaderResourceBinding::uniformBuffer(1,
                                                 QRhiShaderResourceBinding::FragmentStage,
                                                 nullptr), // Lighting UBO
        QRhiShaderResourceBinding::sampledTexture(2,
                                                  QRhiShaderResourceBinding::FragmentStage,
                                                  nullptr, nullptr), // Texture + Sampler
        QRhiShaderResourceBinding::uniformBuffer(3,
                                                 QRhiShaderResourceBinding::VertexStage,
                                                 nullptr, 0, mInstanceBlockAlignedSize)
    });
    if (!mPipelineSrbLayoutDef->create()) {
        qFatal("Failed to create *combined* pipeline SRB layout definition");
    }

    // *** Set the SINGLE combined layout on the pipeline ***
    mBasePipeline->setShaderResourceBindings(mPipelineSrbLayoutDef.get());

    mBasePipeline->setShaderStages({vs, fs});
    mBasePipeline->setVertexInputLayout(inputLayout);
    mBasePipeline->setSampleCount(mSwapChain->sampleCount());
    mBasePipeline->setRenderPassDescriptor(mSwapChainPassDesc);
    mBasePipeline->setCullMode(QRhiGraphicsPipeline::Back);
    mBasePipeline->setFrontFace(QRhiGraphicsPipeline::CW);
    mBasePipeline->setDepthTest(true);
    mBasePipeline->setDepthWrite(true);
    mBasePipeline->setDepthOp(QRhiGraphicsPipeline::Less);
    if (!mBasePipeline->create())
        qFatal("Failed to create graphics pipeline");
}

void RasterizeRenderSystem::findActiveCamera() {
    if (!mWorld) {
        return;
    }
    mActiveCamera = INVALID_ENTITY;
    for (EntityID entity: mWorld->view<CameraComponent, TransformComponent>()) {
        mActiveCamera = entity;
        break;
    }
    if (mActiveCamera == INVALID_ENTITY) {
        qWarning() << "No active camera found in the world!";
    }
}
