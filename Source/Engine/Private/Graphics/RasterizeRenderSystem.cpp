#include "Graphics/RasterizeRenderSystem.h"

#include "Component/CameraComponent.h"
#include "Component/LightComponent.h"
#include "Component/MaterialComponent.h"
#include "Component/RenderableComponent.h"
#include "Component/TransformComponent.h"
#include "Resources/ResourceManager.h"
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

    quint32 alignment = mRhi->ubufAlignment();
    mInstanceBlockAlignedSize = sizeof(InstanceUniformBlock);
    if (mInstanceBlockAlignedSize % alignment != 0) {
        mInstanceBlockAlignedSize += alignment - (mInstanceBlockAlignedSize % alignment);
    }
    qInfo() << "Instance UBO block size (raw):" << sizeof(InstanceUniformBlock)
            << "Aligned size:" << mInstanceBlockAlignedSize
            << "Alignment requirement:" << alignment;

    // 初始化 Camera/Light UBOs, Sampler, Global SRB
    createGlobalResources();
    // 初始化 UBO + CPU buffer
    createInstanceResources();
    createPipelines();

    findActiveCamera();

    if (!mResourceManager->getMeshGpuData(BUILTIN_CUBE_MESH_ID)) {
        mResourceManager->loadMeshFromData(BUILTIN_CUBE_MESH_ID,
                                           DEFAULT_CUBE_VERTICES,
                                           DEFAULT_CUBE_INDICES);
        RhiMeshGpuData *meshGpu = mResourceManager->getMeshGpuData(BUILTIN_CUBE_MESH_ID);
        if (meshGpu) meshGpu->ready = false;
    }
}

void RasterizeRenderSystem::releaseResources() {
    releasePipelines();
    releaseInstanceResources();
    releaseGlobalResources();

    mMaterialBindingsCache.clear();
    mInstanceBindingsCache.clear();

    mRhi = nullptr;
    mSwapChain = nullptr;
    mSwapChainPassDesc = nullptr;
    mWorld.reset();
    mResourceManager.reset();
}

void RasterizeRenderSystem::releasePipelines() {
    mPipelineSrbLayoutDef.clear();
    mBasePipeline.clear();
}

void RasterizeRenderSystem::releaseInstanceResources() {
    mInstanceUbo.reset();
    mInstanceDataBuffer.clear();
}

void RasterizeRenderSystem::releaseGlobalResources() {
    mLightingUbo.reset();
    mCameraUbo.reset();
    mDefaultSampler.reset();
    mGlobalBindings.reset();
}

void RasterizeRenderSystem::resize(QSize size) {
    if (size.isValid()) {
        mOutputSize = size;
    }
}

void RasterizeRenderSystem::draw(QRhiCommandBuffer *cmdBuffer, const QSize &outputSizeInPixels) {
    if (!mWorld || !mResourceManager || !mBasePipeline || !mGlobalBindings || !cmdBuffer || !mInstanceUbo || !mRhi) {
        qWarning() << "draw - Preconditions not met.";
        return;
    }

    // 获取 cube mesh (必须在submitResourceUpdates之后)
    RhiMeshGpuData *cubeMeshGpu = mResourceManager->getMeshGpuData(BUILTIN_CUBE_MESH_ID);
    if (!cubeMeshGpu || !cubeMeshGpu->ready) {
        qWarning() << "Cube mesh data not ready for drawing.";
        return;
    }

    // --- 通用状态 ---
    cmdBuffer->setGraphicsPipeline(mBasePipeline.get());
    cmdBuffer->setViewport({0, 0, (float) mOutputSize.width(), (float) mOutputSize.height()});

    // -- Vertex/Index Buffers --
    QRhiCommandBuffer::VertexInput vtxBinding(cubeMeshGpu->vertexBuffer.get(), 0);
    cmdBuffer->setVertexInput(0, 1, &vtxBinding, cubeMeshGpu->indexBuffer.get(), 0, QRhiCommandBuffer::IndexUInt16);

    // --- Draw ---
    int currentInstanceIndex = 0;

    // 按材质分类
    QHash<QString, QVector<EntityID> > entitiesByMaterial;
    for (EntityID entity: mWorld->view<RenderableComponent, MaterialComponent, TransformComponent>()) {
        const auto *renderable = mWorld->getComponent<RenderableComponent>(entity);
        const auto *matComp = mWorld->getComponent<MaterialComponent>(entity);
        const auto *tfComp = mWorld->getComponent<TransformComponent>(entity); // Need transform to exist
        if (renderable && renderable->isVisible && matComp && tfComp) {
            entitiesByMaterial[matComp->textureResourceId].append(entity);
        }
    }

    for (auto it = entitiesByMaterial.constBegin(); it != entitiesByMaterial.constEnd(); ++it) {
        const QString &textureId = it.key();
        const QVector<EntityID> &entities = it.value();

        // 获取材质
        RhiMaterialGpuData *matGpu = mResourceManager->getMaterialGpuData(textureId);
        if (!matGpu || !matGpu->ready || !matGpu->texture || !mDefaultSampler) {
            qWarning() << "Material resources not ready for texture:" << textureId << "Skipping" << entities.size() <<
                    "entities.";
            currentInstanceIndex += entities.size();
            continue;
        }

        // 绘制材质的所有实体
        for (EntityID entity: entities) {
            if (currentInstanceIndex >= mMaxInstances) break;

            quint32 dynamicOffset = currentInstanceIndex * mInstanceBlockAlignedSize;

            // *** 创建绘制对象的 SRB ***
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
                // Binding 3: Instance UBO Slice (Instance Specific)
                QRhiShaderResourceBinding::uniformBuffer(3,
                                                         QRhiShaderResourceBinding::VertexStage,
                                                         mInstanceUbo.get(), dynamicOffset,
                                                         mInstanceBlockAlignedSize) // Use instance UBO slice
            });

            if (!drawSrb->create()) {
                qWarning() << "Failed to create per-draw SRB for instance" << currentInstanceIndex;
                currentInstanceIndex++;
                continue;
            }

            // --- 完成 SRB ---
            cmdBuffer->setShaderResources(drawSrb.get());

            // --- 提交绘制调用 ---
            cmdBuffer->drawIndexed(cubeMeshGpu->indexCount);

            currentInstanceIndex++;
        }
        if (currentInstanceIndex >= mMaxInstances) break;
    }
}

void RasterizeRenderSystem::submitResourceUpdates(QRhiResourceUpdateBatch *batch) {
    if (!mWorld || !mResourceManager || !batch || !mRhi || !mCameraUbo || !mLightingUbo || !mInstanceUbo) {
        qWarning() << "submitResourceUpdates - Preconditions not met.";
        return;
    }

    // --- 更新相机 UBO (Binding 0) ---
    CameraUniformBlock camData;
    bool cameraOk = false;
    if (mActiveCamera != INVALID_ENTITY) {
        auto *camComp = mWorld->getComponent<CameraComponent>(mActiveCamera);
        auto *camTf = mWorld->getComponent<TransformComponent>(mActiveCamera);
        if (camComp && camTf) {
            QMatrix4x4 viewMatrix;
            viewMatrix.lookAt(camTf->position(), camTf->position() + camTf->rotation().rotatedVector({0, 0, -1}),
                              camTf->rotation().rotatedVector({0, 1, 0}));

            QMatrix4x4 projMatrix;
            projMatrix.perspective(camComp->mFov, camComp->mAspect, camComp->mNearPlane, camComp->mFarPlane);
            projMatrix *= mRhi->clipSpaceCorrMatrix();

            camData.view = viewMatrix.toGenericMatrix<4, 4>();
            camData.projection = projMatrix.toGenericMatrix<4, 4>();
            camData.viewPos = camTf->position();
            cameraOk = true;
        } else {
            qWarning() << "Active camera entity" << mActiveCamera << "missing required components.";
        }
    }
    if (!cameraOk) {
        findActiveCamera();
        QMatrix4x4 identity;
        camData.view = identity.toGenericMatrix<4, 4>();
        camData.projection = identity.toGenericMatrix<4, 4>();
        camData.viewPos = QVector3D(0, 0, 0);
    }
    batch->updateDynamicBuffer(mCameraUbo.get(), 0, sizeof(CameraUniformBlock), &camData);

    // --- 更新灯光 UBO (Binding 1) ---
    LightingUniformBlock lightData;
    bool lightFound = false;
    // 方向光(太阳) 只能使用一个，使用找到的第一个
    for (EntityID entity: mWorld->view<LightComponent, TransformComponent>()) {
        const auto *lightComp = mWorld->getComponent<LightComponent>(entity);
        const auto *lightTf = mWorld->getComponent<TransformComponent>(entity);
        if (lightComp && lightTf && lightComp->type == LightType::Directional) {
            lightData.dirLightDirection = lightTf->rotation().rotatedVector({0, 0, -1}).normalized();
            lightData.dirLightAmbient = lightComp->ambient;
            lightData.dirLightDiffuse = lightComp->diffuse;
            lightData.dirLightSpecular = lightComp->specular;
            lightFound = true;
            break;
        }
    }
    if (!lightFound) {
        lightData.dirLightDirection = QVector3D(0, -1, 0);
        lightData.dirLightAmbient = QVector3D(0.1f, 0.1f, 0.1f);
        lightData.dirLightDiffuse = QVector3D(0.0f, 0.0f, 0.0f);
        lightData.dirLightSpecular = QVector3D(0.0f, 0.0f, 0.0f);
    }
    batch->updateDynamicBuffer(mLightingUbo.get(), 0, sizeof(LightingUniformBlock), &lightData);

    // 构建 Cube Mesh
    RhiMeshGpuData *cubeMeshGpu = mResourceManager->getMeshGpuData(BUILTIN_CUBE_MESH_ID);
    if (!cubeMeshGpu) {
        mResourceManager->loadMeshFromData(BUILTIN_CUBE_MESH_ID, DEFAULT_CUBE_VERTICES, DEFAULT_CUBE_INDICES);
        cubeMeshGpu = mResourceManager->getMeshGpuData(BUILTIN_CUBE_MESH_ID);
        if (cubeMeshGpu)
            cubeMeshGpu->ready = false;
        else
            qWarning() << "Failed to load cube mesh data even on demand.";
    }
    if (cubeMeshGpu && !cubeMeshGpu->ready) {
        mResourceManager->queueMeshUpdate(BUILTIN_CUBE_MESH_ID, batch, DEFAULT_CUBE_VERTICES, DEFAULT_CUBE_INDICES);
    }

    // --- 更新材质 GPU 数据 ---
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


    // --- 收集实例，更新实例 UBO (Binding 3) ---
    int currentInstanceIndex = 0;
    for (EntityID entity: mWorld->view<RenderableComponent, TransformComponent, MaterialComponent>()) {
        if (currentInstanceIndex >= mMaxInstances) {
            qWarning() << "Exceeded max instances per batch:" << mMaxInstances;
            break;
        }
        const auto *renderable = mWorld->getComponent<RenderableComponent>(entity);
        const auto *tfComp = mWorld->getComponent<TransformComponent>(entity);

        if (renderable && renderable->isVisible && tfComp) {
            mInstanceDataBuffer[currentInstanceIndex].model = tfComp->worldMatrix().toGenericMatrix<4, 4>();

            currentInstanceIndex++;
        }
    }

    if (currentInstanceIndex > 0) {
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
    mInstanceUbo.reset(mRhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer,
                                       mMaxInstances * mInstanceBlockAlignedSize)); // Use aligned size
    if (!mInstanceUbo || !mInstanceUbo->create())
        qFatal("Failed to create instance UBO");
    mInstanceUbo->setName(QByteArrayLiteral("Instance Data UBO"));

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

    // --- 管线 ---
    mBasePipeline.reset(mRhi->newGraphicsPipeline());
    mBasePipeline->setName(QByteArrayLiteral("Base Opaque Pipeline"));

    // 使用 nullptr ，因为只是布局定义
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

    mBasePipeline->setShaderResourceBindings(mPipelineSrbLayoutDef.get());

    mBasePipeline->setShaderStages({vs, fs});
    mBasePipeline->setVertexInputLayout(inputLayout);
    mBasePipeline->setSampleCount(mSwapChain->sampleCount());
    mBasePipeline->setRenderPassDescriptor(mSwapChainPassDesc);
    mBasePipeline->setCullMode(QRhiGraphicsPipeline::Back);
    mBasePipeline->setFrontFace(QRhiGraphicsPipeline::CCW);
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
