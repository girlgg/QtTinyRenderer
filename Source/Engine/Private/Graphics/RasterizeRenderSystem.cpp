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

    mResourceManager->loadMeshFromData(BUILTIN_CUBE_MESH_ID,
                                       DEFAULT_CUBE_VERTICES,
                                       DEFAULT_CUBE_INDICES);
    mResourceManager->loadMeshFromData(BUILTIN_PYRAMID_MESH_ID,
                                       DEFAULT_PYRAMID_VERTICES,
                                       DEFAULT_PYRAMID_INDICES);

    RhiMeshGpuData *cubeMeshGpu = mResourceManager->getMeshGpuData(BUILTIN_CUBE_MESH_ID);
    if (cubeMeshGpu)
        cubeMeshGpu->ready = false;
    else
        qWarning() << "Failed to prepare cube mesh data structure.";

    RhiMeshGpuData *pyramidMeshGpu = mResourceManager->getMeshGpuData(BUILTIN_PYRAMID_MESH_ID);
    if (pyramidMeshGpu)
        pyramidMeshGpu->ready = false;
    else
        qWarning() << "Failed to prepare pyramid mesh data structure.";

    qInfo() << "RasterizeRenderSystem initialized.";
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
}

void RasterizeRenderSystem::resize(QSize size) {
    if (size.isValid()) {
        mOutputSize = size;
    }
}

void RasterizeRenderSystem::draw(QRhiCommandBuffer *cmdBuffer, const QSize &outputSizeInPixels) {
    if (!mWorld || !mResourceManager || !mBasePipeline || !cmdBuffer || !mInstanceUbo || !mRhi) {
        qWarning() << "draw - Preconditions not met.";
        return;
    }

    // --- 设置视口和裁剪矩阵 ---
    cmdBuffer->setGraphicsPipeline(mBasePipeline.get());
    cmdBuffer->setViewport({0, 0, (float) mOutputSize.width(), (float) mOutputSize.height()});
    cmdBuffer->setScissor({0, 0, mOutputSize.width(), mOutputSize.height()});

    // 将实体按 Mesh 和 Material分组
    // (Key: MeshID, Value: Map<MaterialID, QVector<EntityID>>)
    QHash<QString, QHash<QString, QVector<EntityID> > > entitiesByMeshThenMaterial;
    int totalEntitiesToRender = 0;

    for (EntityID entity: mWorld->view<RenderableComponent, MeshComponent, MaterialComponent, TransformComponent>()) {
        const auto *renderable = mWorld->getComponent<RenderableComponent>(entity);
        const auto *meshComp = mWorld->getComponent<MeshComponent>(entity);
        const auto *matComp = mWorld->getComponent<MaterialComponent>(entity);
        const auto *tfComp = mWorld->getComponent<TransformComponent>(entity);

        if (renderable && renderable->isVisible && meshComp && matComp && tfComp) {
            QString meshId = meshComp->meshResourceId;
            QString matId = mResourceManager->generateMaterialCacheKey(matComp);

            RhiMeshGpuData *meshGpu = mResourceManager->getMeshGpuData(meshId);
            RhiMaterialGpuData *matGpu = mResourceManager->getMaterialGpuData(matId);
            // 确保资源都已就绪再添加到绘制列表
            if (meshGpu && meshGpu->ready && matGpu && matGpu->ready) {
                // 还需要检查纹理是否就绪
                // 暂时只检查 Albedo
                RhiTextureGpuData *texGpu = mResourceManager->getTextureGpuData(matGpu->albedoId);
                // 确保 Albedo 纹理也准备好了
                if (texGpu && texGpu->ready) {
                    entitiesByMeshThenMaterial[meshId][matId].append(entity);
                    totalEntitiesToRender++;
                } else {
                    qWarning() << "Skipping entity" << entity << "- Albedo texture not ready for material:" << matId;
                }
            } else {
                if (!meshGpu || !meshGpu->ready) {
                    qWarning() << "Skipping entity" << entity << "- Mesh not ready:" << meshId;
                }
                if (!matGpu || !matGpu->ready) {
                    qWarning() << "Skipping entity" << entity << "- Material definition not ready:" << matId;
                }
            }
        }
    }

    int drawnInstanceCount = 0;

    // 获取 cube mesh (必须在submitResourceUpdates之后)
    RhiMeshGpuData *cubeMeshGpu = mResourceManager->getMeshGpuData(BUILTIN_CUBE_MESH_ID);
    if (!cubeMeshGpu || !cubeMeshGpu->ready) {
        qWarning() << "Cube mesh data not ready for drawing.";
        return;
    }

    // -- Vertex/Index Buffers --
    QRhiCommandBuffer::VertexInput vtxBinding(cubeMeshGpu->vertexBuffer.get(), 0);
    cmdBuffer->setVertexInput(0, 1, &vtxBinding, cubeMeshGpu->indexBuffer.get(), 0, QRhiCommandBuffer::IndexUInt16);

    for (auto meshIt = entitiesByMeshThenMaterial.constBegin();
         meshIt != entitiesByMeshThenMaterial.constEnd(); ++meshIt) {
        const QString &meshId = meshIt.key();
        const QHash<QString, QVector<EntityID> > &materials = meshIt.value();

        RhiMeshGpuData *meshGpu = mResourceManager->getMeshGpuData(meshId);
        if (!meshGpu || !meshGpu->ready || meshGpu->indexCount == 0) {
            qWarning() << "draw - Skipping mesh" << meshId << "as its GPU data is not ready or invalid.";
            // 跳过这个 Mesh 的所有材质和实体
            for (const auto &matEntities: materials) {
                // 仍然需要增加 instance 计数器，因为它们在 UBO 中占了位置
                drawnInstanceCount += matEntities.size();
            }
            continue;
        }

        // 为当前 Mesh 设置 VBO 和 IBO
        QRhiCommandBuffer::VertexInput vtxBinding(meshGpu->vertexBuffer.get(), 0);
        // 第一个参数 0: location in shader, 1: number of bindings, &vtxBinding: the binding details
        // meshGpu->indexBuffer.get(): index buffer, 0: offset, QRhiCommandBuffer::IndexUInt16: index type
        cmdBuffer->setVertexInput(0, 1, &vtxBinding, meshGpu->indexBuffer.get(), 0, QRhiCommandBuffer::IndexUInt16);

        // 内层循环：遍历当前 Mesh 的不同 Material
        for (auto matIt = materials.constBegin(); matIt != materials.constEnd(); ++matIt) {
            const QString &materialId = matIt.key();
            const QVector<EntityID> &entities = matIt.value();

            RhiMaterialGpuData *matGpu = mResourceManager->getMaterialGpuData(materialId);
            if (!matGpu || !matGpu->ready) {
                qWarning() << "draw - Skipping material" << materialId << "as its GPU data is not ready (unexpected).";
                drawnInstanceCount += entities.size();
                continue;
            }

            // 获取 Albedo 纹理
            RhiTextureGpuData *texGpu = mResourceManager->getTextureGpuData(matGpu->albedoId);
            if (!texGpu || !texGpu->ready || !texGpu->texture) {
                qWarning() << "draw - Skipping material" << materialId << "due to missing/unready albedo texture:" <<
                        matGpu->albedoId;
                drawnInstanceCount += entities.size();
                continue;
            }
            // 对于这个 Mesh 和 Material 组合下的每个实体进行绘制
            for (EntityID entity: entities) {
                // 检查是否超出 Instance UBO 容量
                if (drawnInstanceCount >= mMaxInstances) {
                    qWarning() << "draw - Reached max instances limit (" << mMaxInstances <<
                            "), skipping further draws.";
                    goto end_draw_loops; // 跳出所有循环
                }
                // 获取当前实例在 Instance UBO 中的动态偏移量
                // drawnInstanceCount 必须是基于所有已处理实体的累计计数，而不是当前批次的索引。
                quint32 dynamicOffset = drawnInstanceCount * mInstanceBlockAlignedSize;
                // --- 设置特定于此绘制调用的 Shader Resource Bindings ---
                // （包括 Camera UBO, Lighting UBO, Texture, Sampler, Instance UBO slice）
                // 注意：Camera 和 Lighting UBO 是全局的，但在这里绑定是为了与纹理和实例数据组合
                QScopedPointer<QRhiShaderResourceBindings> drawSrb(mRhi->newShaderResourceBindings());
                drawSrb->setName(QString("DrawSRB_Mesh%1_Mat%2_Inst%3")
                    .arg(meshId)
                    .arg(materialId.split('/').last())
                    .arg(drawnInstanceCount)
                    .toUtf8());
                drawSrb->setBindings({
                    // Binding 0: Camera UBO (Global)
                    QRhiShaderResourceBinding::uniformBuffer(0,
                                                             QRhiShaderResourceBinding::VertexStage |
                                                             QRhiShaderResourceBinding::FragmentStage,
                                                             mCameraUbo.get()),
                    // Binding 1: Lighting UBO (Global)
                    QRhiShaderResourceBinding::uniformBuffer(1,
                                                             QRhiShaderResourceBinding::FragmentStage,
                                                             mLightingUbo.get()),
                    // Binding 2: Albedo Texture + Sampler (Material specific)
                    QRhiShaderResourceBinding::sampledTexture(2,
                                                              QRhiShaderResourceBinding::FragmentStage,
                                                              texGpu->texture.get(),
                                                              mDefaultSampler.get()),
                    // Binding 3: Instance Data UBO Slice (Instance specific)
                    QRhiShaderResourceBinding::uniformBuffer(3,
                                                             QRhiShaderResourceBinding::VertexStage,
                                                             mInstanceUbo.get(),
                                                             dynamicOffset,
                                                             mInstanceBlockAlignedSize)
                    // QRhiShaderResourceBinding::sampledTexture(4, ... Normal Map ...),
                    // QRhiShaderResourceBinding::sampledTexture(5, ... MetallicRoughness Map ...),
                });

                if (!drawSrb->create()) {
                    qWarning() << "Failed to create per-draw SRB for instance" << drawnInstanceCount << "mesh:" <<
                            meshId << "mat:" << materialId;
                    drawnInstanceCount++;
                    continue;
                }
                cmdBuffer->setShaderResources(drawSrb.get());
                cmdBuffer->drawIndexed(meshGpu->indexCount);
                drawnInstanceCount++;
            }
        }
    }
end_draw_loops:;
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
    memset(&lightData, 0, sizeof(LightingUniformBlock));
    int pointLightIndex = 0;

    for (EntityID entity: mWorld->view<LightComponent, TransformComponent>()) {
        auto *lightComp = mWorld->getComponent<LightComponent>(entity);
        auto *lightTf = mWorld->getComponent<TransformComponent>(entity);

        if (!lightComp || !lightTf) continue;

        switch (lightComp->type) {
            case LightType::Directional:
                // 只处理找到的第一个方向光（或者可以根据需要选择最亮的）
                if (!lightData.dirLight.enabled) {
                    // 方向由 Transform 的 Z 轴负方向决定
                    lightData.dirLight.direction = lightTf->rotation().rotatedVector({0, 0, -1}).normalized();
                    // 直接使用组件中的ADS值，或者基于 color/intensity 计算
                    lightData.dirLight.ambient = lightComp->ambient * lightComp->intensity; // 示例：强度影响环境光
                    lightData.dirLight.diffuse = lightComp->color * lightComp->intensity; // 示例：强度影响漫反射
                    lightData.dirLight.specular = lightComp->specular * lightComp->intensity; // 示例：强度影响高光
                    lightData.dirLight.enabled = 1; // 标记为启用
                }
                break;

            case LightType::Point:
                if (pointLightIndex < MAX_POINT_LIGHTS) {
                    auto &pl = lightData.pointLights[pointLightIndex];
                    pl.position = lightTf->position();
                    pl.color = lightComp->color; // 可以预乘强度: lightComp->color * lightComp->intensity;
                    pl.intensity = lightComp->intensity; // 单独传递强度
                    pl.attenuation = QVector3D(lightComp->constantAttenuation,
                                               lightComp->linearAttenuation,
                                               lightComp->quadraticAttenuation);
                    // pl.ambient = ... (如果需要单独设置)
                    // pl.diffuse = ...
                    // pl.specular = ...
                    pointLightIndex++;
                } else {
                    qWarning() << "Exceeded MAX_POINT_LIGHTS in scene, ignoring extra point light:" << entity;
                }
                break;

            case LightType::Spot:
                // TODO: 添加 Spot Light 处理逻辑 (如果需要)
                // if (spotLightIndex < MAX_SPOT_LIGHTS) { ... }
                break;
        }
    }
    lightData.numPointLights = pointLightIndex;
    // lightData.numSpotLights = spotLightIndex; // 如果有聚光灯

    if (!lightData.dirLight.enabled) {
        qWarning() << "No directional light found in the scene.";
        // 可以设置一个默认的非常弱的或禁用的方向光
        lightData.dirLight.direction = QVector3D(0, -1, 0);
        lightData.dirLight.ambient = QVector3D(0.01f, 0.01f, 0.01f);
        lightData.dirLight.diffuse = QVector3D(0, 0, 0);
        lightData.dirLight.specular = QVector3D(0, 0, 0);
        lightData.dirLight.enabled = 0; // 确保禁用
    }
    // 上传更新后的灯光数据
    batch->updateDynamicBuffer(mLightingUbo.get(), 0, sizeof(LightingUniformBlock), &lightData);

    // 构建 Cube Mesh
    RhiMeshGpuData *cubeMeshGpu = mResourceManager->getMeshGpuData(BUILTIN_CUBE_MESH_ID);
    if (cubeMeshGpu && !cubeMeshGpu->ready) {
        mResourceManager->queueMeshUpdate(BUILTIN_CUBE_MESH_ID, batch, DEFAULT_CUBE_VERTICES, DEFAULT_CUBE_INDICES);
        // queueMeshUpdate 内部会将 ready 设置为 true
    } else if (!cubeMeshGpu) {
        qWarning() << "Cube mesh GPU data is null during update.";
    }
    RhiMeshGpuData *pyramidMeshGpu = mResourceManager->getMeshGpuData(BUILTIN_PYRAMID_MESH_ID);
    if (pyramidMeshGpu && !pyramidMeshGpu->ready) {
        mResourceManager->queueMeshUpdate(BUILTIN_PYRAMID_MESH_ID, batch, DEFAULT_PYRAMID_VERTICES,
                                          DEFAULT_PYRAMID_INDICES);
    } else if (!pyramidMeshGpu) {
        qWarning() << "Pyramid mesh GPU data is null during update.";
    }

    // --- 更新材质 GPU 数据 ---
    QSet<QString> uniqueMaterialIds;
    QSet<QString> requiredTextureIds;

    for (EntityID entity: mWorld->view<RenderableComponent, MaterialComponent>()) {
        const auto *renderable = mWorld->getComponent<RenderableComponent>(entity);
        auto *matComp = mWorld->getComponent<MaterialComponent>(entity);
        if (renderable && renderable->isVisible && matComp) {
            QString matKey = mResourceManager->generateMaterialCacheKey(matComp);
            uniqueMaterialIds.insert(matKey);
            mResourceManager->loadMaterial(matKey, matComp);
            RhiMaterialGpuData *matGpuDef = mResourceManager->getMaterialGpuData(matKey);
            if (matGpuDef) {
                if (!matGpuDef->albedoId.isEmpty()) requiredTextureIds.insert(matGpuDef->albedoId);
                if (!matGpuDef->normalId.isEmpty()) requiredTextureIds.insert(matGpuDef->normalId);
                if (!matGpuDef->metallicRoughnessId.isEmpty())
                    requiredTextureIds.
                            insert(matGpuDef->metallicRoughnessId);
                if (!matGpuDef->aoId.isEmpty()) requiredTextureIds.insert(matGpuDef->aoId);
                if (!matGpuDef->emissiveId.isEmpty()) requiredTextureIds.insert(matGpuDef->emissiveId);
            }
        }
    }
    // 2. 确保所有需要的纹理已加载并排队上传（如果需要）
    for (const QString &textureId: qAsConst(requiredTextureIds)) {
        // loadTexture 应该在 loadMaterial 中被间接调用了，这里主要是检查和触发上传
        RhiTextureGpuData *texGpu = mResourceManager->getTextureGpuData(textureId);
        if (texGpu && !texGpu->ready && !texGpu->sourceImage.isNull()) {
            batch->uploadTexture(texGpu->texture.get(), texGpu->sourceImage);
            texGpu->sourceImage = QImage(); // 清除CPU端图像数据
            texGpu->ready = true;
        } else if (!texGpu) {
            qWarning() << "Texture GPU data not found after load attempt for:" << textureId <<
                    "Was it loaded correctly?";
        }
    }
    // 3. 排队上传材质（主要是标记 ready，因为纹理已单独处理）
    for (const QString &materialId: qAsConst(uniqueMaterialIds)) {
        RhiMaterialGpuData *matGpu = mResourceManager->getMaterialGpuData(materialId);
        if (matGpu && !matGpu->ready) {
            bool allTexturesReady = true;
            if (!matGpu->albedoId.isEmpty())
                allTexturesReady &= (mResourceManager->getTextureGpuData(matGpu->albedoId) && mResourceManager->
                                     getTextureGpuData(matGpu->albedoId)->ready);
            // ... 检查其他纹理 ...
            if (allTexturesReady) {
                matGpu->ready = true; // 标记材质就绪
            } else {
                qWarning() << "Material" << materialId << "not ready because some textures are not ready.";
            }
        } else if (!matGpu) {
            qWarning() << "Material GPU data still null after load attempt for:" << materialId;
        }
    }

    // --- 收集实例，更新实例 UBO (Binding 3) ---
    int currentInstanceIndex = 0;
    for (EntityID entity: mWorld->view<RenderableComponent, TransformComponent, MaterialComponent, MeshComponent>()) {
        if (currentInstanceIndex >= mMaxInstances) {
            qWarning() << "Exceeded max instances per batch:" << mMaxInstances;
            break;
        }
        const auto *renderable = mWorld->getComponent<RenderableComponent>(entity);
        const auto *tfComp = mWorld->getComponent<TransformComponent>(entity);

        // 选择性地在这里也检查 Material 和 Mesh 是否有效，以减少 UBO 更新量
        // const auto *matComp = mWorld->getComponent<MaterialComponent>(entity);
        // const auto *meshComp = mWorld->getComponent<MeshComponent>(entity);
        // RhiMaterialGpuData* matGpu = mResourceManager->getMaterialGpuData(mResourceManager->generateMaterialCacheKey(matComp));
        // RhiMeshGpuData* meshGpu = mResourceManager->getMeshGpuData(meshComp->meshResourceId);

        if (renderable && renderable->isVisible && tfComp /*&& matGpu && matGpu->ready && meshGpu && meshGpu->ready*/) {
            mInstanceDataBuffer[currentInstanceIndex].model = tfComp->worldMatrix().toGenericMatrix<4, 4>();
            // 如果需要法线矩阵等，也在这里计算并填充
            // mInstanceDataBuffer[currentInstanceIndex].normalMatrix = ...;
            currentInstanceIndex++;
        }
    }

    if (currentInstanceIndex > 0) {
        batch->updateDynamicBuffer(mInstanceUbo.get(), 0,
                                   currentInstanceIndex * mInstanceBlockAlignedSize,
                                   mInstanceDataBuffer.data());
    }
    qInfo() << "Submitted updates for" << currentInstanceIndex << "instances.";
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

    // --- rhi无法使用全局 SRB ---
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
