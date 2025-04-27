#include "RenderGraph/BasePass.h"

#include "CommonRender.h"
#include "Component/CameraComponent.h"
#include "Component/LightComponent.h"
#include "Component/MeshComponent.h"
#include "Component/RenderableComponent.h"
#include "Component/TransformComponent.h"
#include "Component/MaterialComponent.h"
#include "RenderGraph/RGBuilder.h"
#include "Resources/ResourceManager.h"
#include "Scene/World.h"

BasePass::BasePass(const QString &name): RGPass(name) {
}

void BasePass::setup(RGBuilder &builder) {
    qInfo() << "BasePass::setup -" << name();
    mRhi = builder.rhi();
    mResourceManager = builder.resourceManager();
    mWorld = builder.world();

    if (!mRhi || !mResourceManager || !mWorld) {
        qCritical("BasePass::setup - RHI, ResourceManager, or World is null!");
        return;
    }

    const QSize outputSize = builder.outputSize();
    if (!outputSize.isValid()) {
        qWarning("BasePass::setup - Invalid output size provided by builder.");
        return;
    }
    qInfo() << "  Output size:" << outputSize;

    // 声明输出资源
    mOutput.baseColor = builder.writeTexture("BaseColor",
                                             outputSize,
                                             QRhiTexture::RGBA8,
                                             1,
                                             QRhiTexture::RenderTarget | QRhiTexture::UsedAsTransferSource);
    if (!mOutput.baseColor.isValid()) {
        qCritical("BasePass::setup - Failed to declare BaseColor texture resource.");
        return;
    }
    qInfo() << "  Declared Write: RGTexture 'BaseColor'";
    mOutput.depthStencil = builder.writeDepthStencil("DepthStencil", outputSize, 1); // Name, Size, Sample Count
    if (!mOutput.depthStencil.isValid()) {
        qCritical("BasePass::setup - Failed to declare DepthStencil buffer resource.");
        return;
    }
    qInfo() << "  Declared Write: RGRenderBuffer 'DepthStencil'";
    // 声明 Uniform Buffers (暂存，每帧动态更新)
    mCameraUboRef = builder.createBuffer("CameraUBO",
                                         QRhiBuffer::Dynamic,
                                         QRhiBuffer::UniformBuffer,
                                         sizeof(CameraUniformBlock));
    if (!mCameraUboRef.isValid()) {
        qCritical("BasePass::setup - Failed to declare CameraUBO.");
        return;
    }
    qInfo() << "  Declared Buffer: 'CameraUBO'";

    mLightingUboRef = builder.createBuffer("LightingUBO",
                                           QRhiBuffer::Dynamic,
                                           QRhiBuffer::UniformBuffer,
                                           sizeof(LightingUniformBlock));
    if (!mLightingUboRef.isValid()) {
        qCritical("BasePass::setup - Failed to declare LightingUBO.");
        return;
    }
    qInfo() << "  Declared Buffer: 'LightingUBO'";

    // Instance UBO
    const quint32 alignment = mRhi->ubufAlignment();
    mInstanceBlockAlignedSize = sizeof(InstanceUniformBlock);
    if (mInstanceBlockAlignedSize % alignment != 0) {
        mInstanceBlockAlignedSize += alignment - (mInstanceBlockAlignedSize % alignment);
    }
    qInfo() << "  Instance UBO Block Size:" << sizeof(InstanceUniformBlock) << "Aligned Size:" <<
            mInstanceBlockAlignedSize << "Alignment:" << alignment;

    mInstanceUboRef = builder.createBuffer("InstanceUBO", QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer,
                                           mMaxInstances * mInstanceBlockAlignedSize);
    if (!mInstanceUboRef.isValid()) {
        qCritical("BasePass::setup - Failed to declare InstanceUBO.");
        return;
    }
    qInfo() << "  Declared Buffer: 'InstanceUBO' (Capacity:" << mMaxInstances << ")";
    mInstanceDataBuffer.resize(mMaxInstances);

    // --- 设置 Sampler ---
    mDefaultSamplerRef = builder.setupSampler("DefaultSampler",
                                              QRhiSampler::Linear, QRhiSampler::Linear, QRhiSampler::Linear,
                                              QRhiSampler::Repeat, QRhiSampler::Repeat);
    if (!mDefaultSamplerRef.isValid()) {
        qCritical("BasePass::setup - Failed to setup DefaultSampler.");
        return;
    }
    qInfo() << "  Setup Sampler: 'DefaultSampler'";

    // --- 设置 RenderTarget ---
    if (!mOutput.baseColor.isValid() || !mOutput.depthStencil.isValid()) {
        qWarning("BasePass::setup - Failed to create output color or depth stencil resources.");
        return;
    }
    mRenderTargetRef = builder.setupRenderTarget("BasePassRT", {mOutput.baseColor},
                                                 RGRenderBufferRef{mOutput.depthStencil});
    if (!mRenderTargetRef.isValid()) {
        qCritical("BasePass::setup - Failed to setup render target 'BasePassRT'.");
        return;
    }
    qInfo() << "  Setup RenderTarget: 'BasePassRT' using BaseColor and DepthStencil";
    // --- 设置 Pipeline 状态 ---
    const quint32 vertexStride = sizeof(VertexData);
    QRhiVertexInputLayout inputLayout;
    inputLayout.setBindings({QRhiVertexInputBinding(vertexStride)});
    inputLayout.setAttributes({
        QRhiVertexInputAttribute(0, 0, QRhiVertexInputAttribute::Float3, offsetof(VertexData, position)),
        QRhiVertexInputAttribute(0, 1, QRhiVertexInputAttribute::Float3, offsetof(VertexData, normal)),
        QRhiVertexInputAttribute(0, 2, QRhiVertexInputAttribute::Float2, offsetof(VertexData, texCoord)),
        QRhiVertexInputAttribute(0, 3, QRhiVertexInputAttribute::Float3, offsetof(VertexData, tangent))
    });
    qInfo() << "  Setup Vertex Input Layout with Tangent.";

    // Load Shaders
    ShaderBundle::getInstance()->loadShader("Shaders/pbr", {
                                                {":/shaders/pbr.vert.qsb", QRhiShaderStage::Vertex},
                                                {":/shaders/pbr.frag.qsb", QRhiShaderStage::Fragment}
                                            });
    QRhiShaderStage vs = ShaderBundle::getInstance()->getShaderStage("Shaders/pbr", QRhiShaderStage::Vertex);
    QRhiShaderStage fs = ShaderBundle::getInstance()->getShaderStage("Shaders/pbr", QRhiShaderStage::Fragment);
    if (!vs.shader().isValid() || !fs.shader().isValid()) {
        qCritical("BasePass::setup - Failed to load base shaders.");
        return;
    }
    qInfo() << "  Loaded Shaders: 'Shaders/pbr'";

    // --- SRB Layout (Shader Resource Bindings) ---
    QVector bindingsLayout = {
        // Binding 0: Camera UBO (VS+FS)
        QRhiShaderResourceBinding::uniformBuffer(
            0, QRhiShaderResourceBinding::VertexStage | QRhiShaderResourceBinding::FragmentStage, nullptr),
        // Binding 1: Lighting UBO (FS)
        QRhiShaderResourceBinding::uniformBuffer(1, QRhiShaderResourceBinding::FragmentStage, nullptr),
        // Binding 2: Albedo Texture + Sampler (FS)
        QRhiShaderResourceBinding::sampledTexture(2, QRhiShaderResourceBinding::FragmentStage, nullptr, nullptr),
        // Binding 3: Instance UBO (VS) - Dynamic Offset
        QRhiShaderResourceBinding::uniformBuffer(3, QRhiShaderResourceBinding::VertexStage, nullptr, 0,
                                                 mInstanceBlockAlignedSize),
        // Binding 4: Normal Map + Sampler
        QRhiShaderResourceBinding::sampledTexture(4, QRhiShaderResourceBinding::FragmentStage, nullptr, nullptr),
        // Binding 5: Metallic/Roughness Map + Sampler
        QRhiShaderResourceBinding::sampledTexture(5, QRhiShaderResourceBinding::FragmentStage, nullptr, nullptr),
        // Binding 6: Ambient Occlusion Map + Sampler
        QRhiShaderResourceBinding::sampledTexture(6, QRhiShaderResourceBinding::FragmentStage, nullptr, nullptr),
        // Binding 7: Emissive Map + Sampler
        QRhiShaderResourceBinding::sampledTexture(7, QRhiShaderResourceBinding::FragmentStage, nullptr, nullptr)
    };

    mBaseSrbLayoutRef = builder.setupShaderResourceBindings("PbrPipelineSRBLayout", bindingsLayout);
    if (!mBaseSrbLayoutRef.isValid()) {
        qCritical("BasePass::setup - Failed to setup SRB Layout 'PbrPipelineSRBLayout'.");
        return;
    }
    qInfo() << "  Setup SRB Layout: 'PbrPipelineSRBLayout'";

    mPipelineRef = builder.setupGraphicsPipeline("BasePipeline",
                                                 mBaseSrbLayoutRef,
                                                 mRenderTargetRef,
                                                 {vs, fs},
                                                 inputLayout,
                                                 QRhiGraphicsPipeline::Triangles,
                                                 QRhiGraphicsPipeline::Back,
                                                 QRhiGraphicsPipeline::CCW,
                                                 true, true, QRhiGraphicsPipeline::Less
    );
    if (!mPipelineRef.isValid()) {
        qCritical("BasePass::setup - Failed to setup graphics pipeline 'BasePipeline'.");
        return;
    }
    qInfo() << "  Setup Graphics Pipeline: 'BasePipeline'";
    qInfo() << "BasePass::setup finished successfully.";
}

void BasePass::execute(QRhiCommandBuffer *cmdBuffer) {
    // --- 获取所需 RHI 对象 ---
    QRhiGraphicsPipeline *pipeline = mPipelineRef.get();
    QRhiRenderTarget *renderTarget = mRenderTargetRef.get();
    QRhiBuffer *cameraUbo = mCameraUboRef.get();
    QRhiBuffer *lightingUbo = mLightingUboRef.get();
    QRhiBuffer *instanceUbo = mInstanceUboRef.get();
    QRhiSampler *defaultSampler = mDefaultSamplerRef.get();

    if (!pipeline || !renderTarget || !cameraUbo || !lightingUbo || !instanceUbo || !defaultSampler || !
        mWorld || !mResourceManager || !mRhi) {
        qWarning(
            "BasePass::execute [%s] - Prerequisites not met (RHI objects, managers, or world missing/invalid). Skipping.",
            qPrintable(name()));
        qWarning() << "  Pipeline:" << pipeline << "(Ref valid:" << mPipelineRef.isValid() << ")";
        qWarning() << "  RT:" << renderTarget << "(Ref valid:" << mRenderTargetRef.isValid() << ")";
        qWarning() << "  CamUBO:" << cameraUbo << "(Ref valid:" << mCameraUboRef.isValid() << ")";
        qWarning() << "  LightUBO:" << lightingUbo << "(Ref valid:" << mLightingUboRef.isValid() << ")";
        qWarning() << "  InstUBO:" << instanceUbo << "(Ref valid:" << mInstanceUboRef.isValid() << ")";
        qWarning() << "  Sampler:" << defaultSampler << "(Ref valid:" << mDefaultSamplerRef.isValid() << ")";
        qWarning() << "  World:" << mWorld.data() << " ResMgr:" << mResourceManager.data() << " RHI:" << mRhi;
        return;
    }

    bool rtAttachmentsValid = true;
    if (mOutput.baseColor.isValid()) {
        if (!mOutput.baseColor.get()) {
            qWarning("  - BaseColor texture RHI object is null.");
            rtAttachmentsValid = false;
        }
    } else {
        qWarning("  - BaseColor Ref is invalid.");
        rtAttachmentsValid = false;
    }
    if (mOutput.depthStencil.isValid()) {
        if (!mOutput.depthStencil.get()) {
            qWarning("  - DepthStencil buffer RHI object is null.");
            rtAttachmentsValid = false;
        }
    } else {
        qWarning("  - DepthStencil Ref is invalid.");
        rtAttachmentsValid = false;
    }
    if (!rtAttachmentsValid) {
        qWarning("BasePass::execute [%s] - Render target attachments invalid. Skipping.", qPrintable(name()));
        return;
    }

    // --- 更新资源 ---
    QRhiResourceUpdateBatch *resourceBatch = mRhi->nextResourceUpdateBatch();
    if (!resourceBatch) {
        qWarning("BasePass::execute [%s] - Failed to get resource update batch.", qPrintable(name()));
        return;
    }
    updateUniforms(resourceBatch);

    int currentInstanceIndex = 0;
    QHash<QString, QHash<QString, QVector<EntityID> > > entitiesByMeshThenMaterial;
    for (EntityID entity: mWorld->view<RenderableComponent, MeshComponent, MaterialComponent, TransformComponent>()) {
        const auto *renderable = mWorld->getComponent<RenderableComponent>(entity);
        const auto *meshComp = mWorld->getComponent<MeshComponent>(entity);
        const auto *matComp = mWorld->getComponent<MaterialComponent>(entity);
        const auto *tfComp = mWorld->getComponent<TransformComponent>(entity);

        if (renderable && renderable->isVisible && meshComp && !meshComp->meshResourceId.isEmpty()
            && matComp && tfComp) {
            if (currentInstanceIndex >= mMaxInstances) {
                qWarning("BasePass::execute [%s] - Exceeded max instances (%d). Some objects may not be drawn.",
                         qPrintable(name()), mMaxInstances);
                break;
            }

            QString meshId = meshComp->meshResourceId;
            QString materialCacheKey = mResourceManager->generateMaterialCacheKey(matComp);

            RhiMaterialGpuData *matGpu = mResourceManager->getMaterialGpuData(materialCacheKey);
            if (!matGpu) {
                mResourceManager->loadMaterial(materialCacheKey, matComp);
                matGpu = mResourceManager->getMaterialGpuData(materialCacheKey);
                if (!matGpu) {
                    qWarning("BasePass::execute [%s] - Failed to load material '%s' for entity %lld.",
                             qPrintable(name()), qPrintable(materialCacheKey), entity);
                    continue; // Skip this entity
                }
                qInfo() << "Loaded new material:" << materialCacheKey;
            }
            if (!matGpu->ready) {
                mResourceManager->queueMaterialUpdate(materialCacheKey, resourceBatch);
            }
            RhiTextureGpuData *albedoTexGpu = nullptr;
            if (matGpu && !matGpu->albedoId.isEmpty()) {
                albedoTexGpu = mResourceManager->getTextureGpuData(matGpu->albedoId);
            }
            if (!albedoTexGpu || !albedoTexGpu->ready) {
                QString texId = matGpu ? matGpu->albedoId : "(unknown)";
                if (matGpu && !matGpu->albedoId.isEmpty()) {
                    qInfo(
                        "BasePass::execute [%s] - Albedo texture '%s' for material '%s' not ready/found, queuing load/update.",
                        qPrintable(name()), qPrintable(texId), qPrintable(materialCacheKey));
                    mResourceManager->queueTextureUpdate(texId, resourceBatch);
                } else {
                    matGpu->albedoId = DEFAULT_WHITE_TEXTURE_ID;
                    albedoTexGpu = mResourceManager->getTextureGpuData(DEFAULT_WHITE_TEXTURE_ID);
                    if (!albedoTexGpu || !albedoTexGpu->ready) {
                        mResourceManager->queueTextureUpdate(DEFAULT_WHITE_TEXTURE_ID, resourceBatch);
                        qWarning("BasePass::execute [%s] - Fallback white texture not ready, queuing update.",
                                 qPrintable(name()));
                    }
                }
            }

            RhiMeshGpuData *meshGpu = mResourceManager->getMeshGpuData(meshId);
            if (!meshGpu) {
                qWarning(
                    "BasePass::execute [%s] - Mesh GPU data for ID '%s' not found (entity %lld). Did you call loadMeshFromData?",
                    qPrintable(name()), qPrintable(meshId), entity);
                continue;
            }
            if (!meshGpu->ready) {
                qInfo() << "BasePass::execute [" << name() << "] - Mesh '" << meshId <<
                        "' not ready, queuing update...";
                bool queued = mResourceManager->queueMeshUpdate(meshId, resourceBatch);
                if (!queued) {
                    qWarning("BasePass::execute [%s] - Failed to queue mesh update for '%s'.", qPrintable(name()),
                             qPrintable(meshId));
                    continue;
                }
                meshGpu = mResourceManager->getMeshGpuData(meshId);
                if (!meshGpu || !meshGpu->ready) {
                    qWarning(
                        "BasePass::execute [%s] - Mesh '%s' still not ready after queueing update. Skipping entity %lld.",
                        qPrintable(name()), qPrintable(meshId), entity);
                    continue;
                }
                qInfo() << "BasePass::execute [" << name() << "] - Mesh '" << meshId <<
                        "' successfully queued and marked ready.";
            }
            if (meshGpu && meshGpu->ready && matGpu && matGpu->ready && albedoTexGpu && albedoTexGpu->ready &&
                meshGpu->vertexBuffer && meshGpu->indexBuffer) {
                entitiesByMeshThenMaterial[meshId][materialCacheKey].append(entity);
                if (tfComp) {
                    mInstanceDataBuffer[currentInstanceIndex].model = tfComp->worldMatrix().toGenericMatrix<4, 4>();
                } else {
                    qWarning() << "BasePass::execute - Entity" << entity <<
                            "missing TransformComponent when preparing instance data.";
                    continue;
                }
                currentInstanceIndex++;
            } else {
                if (!meshGpu->ready)
                    qWarning() << "Skipping entity" << entity << "because mesh" << meshId << "not ready.";
                else if (!matGpu || !matGpu->ready)
                    qWarning() << "Skipping entity" << entity << "because material" << materialCacheKey << "not ready.";
                else if (!albedoTexGpu || !albedoTexGpu->ready)
                    qWarning() << "Skipping entity" << entity << "because albedo texture" << (
                        matGpu ? matGpu->albedoId : "N/A") << "not ready.";
                else if (!meshGpu->vertexBuffer)
                    qWarning() << "Skipping entity" << entity << "because mesh" << meshId << "vertex buffer is null.";
                else if (!meshGpu->indexBuffer)
                    qWarning() << "Skipping entity" << entity << "because mesh" << meshId << "index buffer is null.";
            }
        } else if (meshComp && meshComp->meshResourceId.isEmpty()) {
            qWarning("Entity %lld has MeshComponent but no meshResourceId set.", entity);
        }
    }

    uploadInstanceData(resourceBatch, currentInstanceIndex);

    cmdBuffer->resourceUpdate(resourceBatch);

    // --- Begin Render Pass ---
    const QColor clearColor = QColor::fromRgbF(0.2f, 0.3f, 0.2f, 1.0f);
    const QRhiDepthStencilClearValue dsClearValue = {1.0f, 0};
    cmdBuffer->beginPass(renderTarget, clearColor, dsClearValue, nullptr);

    // --- 设置 Pipeline ---
    cmdBuffer->setGraphicsPipeline(pipeline);
    const QSize outputSize = renderTarget->pixelSize();
    cmdBuffer->setViewport({0, 0, (float) outputSize.width(), (float) outputSize.height()});
    cmdBuffer->setScissor({0, 0, outputSize.width(), outputSize.height()});

    // --- 绘制实体 ---
    int totalInstancesDrawnThisPass = 0;
    for (auto meshIt = entitiesByMeshThenMaterial.constBegin();
         meshIt != entitiesByMeshThenMaterial.constEnd(); ++meshIt) {
        const QString &meshId = meshIt.key();
        const QHash<QString, QVector<EntityID> > &materials = meshIt.value();
        RhiMeshGpuData *meshGpu = mResourceManager->getMeshGpuData(meshId);
        if (!meshGpu || !meshGpu->ready || !meshGpu->vertexBuffer || !meshGpu->indexBuffer || meshGpu->indexCount ==
            0) {
            qWarning("BasePass::execute [%s] - Skipping draw for mesh '%s': Mesh GPU data invalid or not ready.",
                     qPrintable(name()), qPrintable(meshId));
            for (const auto &matEntities: materials)
                totalInstancesDrawnThisPass += matEntities.size();
            continue;
        }

        // 设置顶点和索引Buffer
        QRhiCommandBuffer::VertexInput vtxBinding(meshGpu->vertexBuffer.get(), 0);
        cmdBuffer->setVertexInput(0, 1, &vtxBinding, meshGpu->indexBuffer.get(), 0, QRhiCommandBuffer::IndexUInt16);

        for (auto matIt = materials.constBegin(); matIt != materials.constEnd(); ++matIt) {
            const QString &materialId = matIt.key();
            const QVector<EntityID> &entitiesInBatch = matIt.value();

            if (entitiesInBatch.isEmpty()) {
                continue;
            }

            RhiMaterialGpuData *matGpu = mResourceManager->getMaterialGpuData(materialId);
            const quint32 currentBatchInstanceCount = entitiesInBatch.size();

            if (!matGpu || !matGpu->ready) {
                qWarning(
                    "BasePass::execute [%s] - Skipping draw for material '%s': Material GPU data invalid or not ready.",
                    qPrintable(name()), qPrintable(materialId));
                totalInstancesDrawnThisPass += currentBatchInstanceCount; // Advance count even if skipping
                continue;
            }

            auto getTexOrDefault = [&](const QString &id, const QString &defaultId) -> RhiTextureGpuData * {
                RhiTextureGpuData *texData = mResourceManager->getTextureGpuData(id);
                if (texData && texData->ready && texData->texture) {
                    return texData;
                }
                qDebug() << "BasePass: Texture" << id << "not ready or found, using default" << defaultId;
                RhiTextureGpuData *defaultTexData = mResourceManager->getTextureGpuData(defaultId);
                if (!defaultTexData || !defaultTexData->ready || !defaultTexData->texture) {
                    qWarning() << "BasePass: Default texture" << defaultId << "is also not ready!";
                    if (defaultId != DEFAULT_WHITE_TEXTURE_ID) {
                        defaultTexData = mResourceManager->getTextureGpuData(DEFAULT_WHITE_TEXTURE_ID);
                        if (!defaultTexData || !defaultTexData->ready || !defaultTexData->texture) {
                            qCritical("BasePass: CRITICAL - Default white texture not ready!");
                            return nullptr;
                        }
                    } else {
                        qCritical("BasePass: CRITICAL - Default white texture not ready!");
                        return nullptr;
                    }
                }
                return defaultTexData;
            };

            RhiTextureGpuData *albedoTexGpu = getTexOrDefault(matGpu->albedoId, DEFAULT_WHITE_TEXTURE_ID);
            RhiTextureGpuData *normalTexGpu = getTexOrDefault(matGpu->normalId, DEFAULT_NORMAL_MAP_ID);
            RhiTextureGpuData *metalRoughTexGpu = getTexOrDefault(matGpu->metallicRoughnessId,
                                                                  DEFAULT_METALROUGH_TEXTURE_ID);
            RhiTextureGpuData *aoTexGpu = getTexOrDefault(matGpu->aoId, DEFAULT_WHITE_TEXTURE_ID);
            RhiTextureGpuData *emissiveTexGpu = getTexOrDefault(matGpu->emissiveId, DEFAULT_BLACK_TEXTURE_ID);

            if (!albedoTexGpu) {
                qWarning(
                    "BasePass::execute [%s] - Could not get even default Albedo texture for material '%s'. Skipping draw.",
                    qPrintable(name()), qPrintable(materialId));
                totalInstancesDrawnThisPass += currentBatchInstanceCount;
                continue;
            }
            if (!normalTexGpu || !metalRoughTexGpu || !aoTexGpu || !emissiveTexGpu) {
                qWarning(
                    "BasePass::execute [%s] - Failed to get one or more default textures for material '%s'. Draw might be incorrect.",
                    qPrintable(name()), qPrintable(materialId));
            }

            QScopedPointer<QRhiShaderResourceBindings> drawSrb(mRhi->newShaderResourceBindings());
            if (!drawSrb) {
                qWarning("BasePass::execute [%s] - Failed to create new QRhiShaderResourceBindings.",
                         qPrintable(name()));
                totalInstancesDrawnThisPass += currentBatchInstanceCount;
                continue;
            }
            if (currentBatchInstanceCount == 0) continue;
            drawSrb->setBindings({
                // Binding 0: Camera UBO
                QRhiShaderResourceBinding::uniformBuffer(
                    0, QRhiShaderResourceBinding::VertexStage | QRhiShaderResourceBinding::FragmentStage, cameraUbo),
                // Binding 1: Lighting UBO
                QRhiShaderResourceBinding::uniformBuffer(1, QRhiShaderResourceBinding::FragmentStage, lightingUbo),
                // Binding 2: Albedo Map
                QRhiShaderResourceBinding::sampledTexture(2, QRhiShaderResourceBinding::FragmentStage,
                                                          albedoTexGpu->texture.get(), defaultSampler),
                // Binding 3: Instance UBO (Dynamic Offset)
                QRhiShaderResourceBinding::uniformBuffer(3, QRhiShaderResourceBinding::VertexStage, instanceUbo,
                                                         totalInstancesDrawnThisPass * mInstanceBlockAlignedSize,
                                                         currentBatchInstanceCount * mInstanceBlockAlignedSize),
                // Binding 4: Normal Map
                QRhiShaderResourceBinding::sampledTexture(4, QRhiShaderResourceBinding::FragmentStage,
                                                          normalTexGpu->texture.get(), defaultSampler),
                // Binding 5: Metallic/Roughness Map
                QRhiShaderResourceBinding::sampledTexture(5, QRhiShaderResourceBinding::FragmentStage,
                                                          metalRoughTexGpu->texture.get(), defaultSampler),
                // Binding 6: AO Map
                QRhiShaderResourceBinding::sampledTexture(6, QRhiShaderResourceBinding::FragmentStage,
                                                          aoTexGpu->texture.get(), defaultSampler),
                // Binding 7: Emissive Map
                QRhiShaderResourceBinding::sampledTexture(7, QRhiShaderResourceBinding::FragmentStage,
                                                          emissiveTexGpu->texture.get(), defaultSampler)
            });
            if (!drawSrb->create()) {
                qWarning("BasePass::execute [%s] - Failed to create draw SRB for mesh '%s', material '%s'.",
                         qPrintable(name()), qPrintable(meshId), qPrintable(materialId));
                totalInstancesDrawnThisPass += currentBatchInstanceCount;
                continue;
            }

            cmdBuffer->setShaderResources(drawSrb.get());

            // 绘制实例
            if (!entitiesInBatch.isEmpty()) {
                cmdBuffer->drawIndexed(meshGpu->indexCount, currentBatchInstanceCount);
            }

            totalInstancesDrawnThisPass += currentBatchInstanceCount;
        }
    }
    if (totalInstancesDrawnThisPass != currentInstanceIndex) {
        qWarning(
            "Mismatch between instances prepared (%d) and instances submitted for drawing (%d). Check skipping logic.",
            currentInstanceIndex, totalInstancesDrawnThisPass);
    } else {
        qInfo() << "BasePass submitted" << totalInstancesDrawnThisPass << "instances.";
    }
    // End Render Pass
    cmdBuffer->endPass();
}

void BasePass::updateUniforms(QRhiResourceUpdateBatch *batch) {
    if (!mWorld || !mCameraUboRef.isValid() || !mLightingUboRef.isValid() || !mRhi) {
        qWarning("BasePass::updateUniforms - World or UBO refs are invalid.");
        return;
    }

    QRhiBuffer *cameraUbo = mCameraUboRef.get();
    QRhiBuffer *lightingUbo = mLightingUboRef.get();
    if (!cameraUbo || !lightingUbo) {
        qWarning("BasePass::updateUniforms - UBO buffers not created yet.");
        return;
    }

    // --- Update Camera UBO ---
    CameraUniformBlock camData;
    bool cameraFoundAndValid = false;
    if (mActiveCamera == INVALID_ENTITY) {
        findActiveCamera();
    }

    if (mActiveCamera != INVALID_ENTITY) {
        auto *camComp = mWorld->getComponent<CameraComponent>(mActiveCamera);
        auto *camTf = mWorld->getComponent<TransformComponent>(mActiveCamera);
        if (camComp && camTf) {
            QMatrix4x4 viewMatrix;
            QVector3D eye = camTf->position();
            QVector3D center = eye + camTf->forward();
            QVector3D up = camTf->up();
            viewMatrix.lookAt(eye, center, up);

            QMatrix4x4 projMatrix;
            const QSize outputPixelSize = mOutput.baseColor.pixelSize(); // Use graph output size
            float aspect = 1.0f;
            if (outputPixelSize.isValid() && outputPixelSize.height() > 0) {
                aspect = outputPixelSize.width() / (float) outputPixelSize.height();
            } else {
                qWarning("BasePass::updateUniforms - Invalid output size for aspect ratio calculation.");
            }
            projMatrix.perspective(camComp->mFov, aspect, camComp->mNearPlane, camComp->mFarPlane);
            projMatrix *= mRhi->clipSpaceCorrMatrix(); // Apply correction matrix

            camData.view = viewMatrix.toGenericMatrix<4, 4>();
            camData.projection = projMatrix.toGenericMatrix<4, 4>();
            camData.viewPos = camTf->position();
            cameraFoundAndValid = true;
        } else {
            qWarning(
                "BasePass::updateUniforms - Active camera entity %lld missing CameraComponent or TransformComponent.",
                mActiveCamera);
            mActiveCamera = INVALID_ENTITY;
        }
    }
    if (!cameraFoundAndValid) {
        qWarning("BasePass::updateUniforms - Using default camera matrices.");
        QMatrix4x4 identity;
        camData.view = identity.toGenericMatrix<4, 4>();
        camData.projection = identity.toGenericMatrix<4, 4>();
        camData.viewPos = QVector3D(0, 0, 5);
    }
    batch->updateDynamicBuffer(mCameraUboRef.get(), 0, sizeof(CameraUniformBlock), &camData);

    // --- 更新 Lighting UBO ---
    LightingUniformBlock lightData;
    memset(&lightData, 0, sizeof(LightingUniformBlock));
    int pointLightCount = 0;
    bool dirLightSet = false;

    for (EntityID entity: mWorld->view<LightComponent, TransformComponent>()) {
        auto *lightComp = mWorld->getComponent<LightComponent>(entity);
        auto *lightTf = mWorld->getComponent<TransformComponent>(entity);
        if (!lightComp || !lightTf) continue;

        switch (lightComp->type) {
            case LightType::Directional:
                if (!dirLightSet) {
                    lightData.dirLight.direction = lightTf->rotation().rotatedVector({0, 0, -1}).normalized();
                    lightData.dirLight.color = lightComp->color * lightComp->intensity;
                    lightData.dirLight.enabled = 1;
                    dirLightSet = true;
                } else {
                    qWarning("BasePass::updateUniforms - Multiple directional lights found. Using the first one.");
                }
                break;
            case LightType::Point:
                if (pointLightCount < MAX_POINT_LIGHTS) {
                    auto &pl = lightData.pointLights[pointLightCount];
                    pl.position = lightTf->position();
                    pl.color = lightComp->color * lightComp->intensity;
                    pl.constant = lightComp->constantAttenuation;
                    pl.linear = lightComp->linearAttenuation;
                    pl.quadratic = lightComp->quadraticAttenuation;
                    pointLightCount++;
                } else {
                    qWarning("BasePass::updateUniforms - Exceeded maximum point lights (%d).", MAX_POINT_LIGHTS);
                }
                break;
        }
    }
    lightData.numPointLights = pointLightCount;
    if (!dirLightSet) {
        lightData.dirLight.enabled = 0;
    }
    batch->updateDynamicBuffer(mLightingUboRef.get(), 0, sizeof(LightingUniformBlock), &lightData);
}

void BasePass::uploadInstanceData(QRhiResourceUpdateBatch *batch, int instanceCount) {
    if (!mInstanceUboRef.isValid()) {
        qWarning("BasePass::uploadInstanceData - Instance UBO Ref is invalid.");
        return;
    }
    QRhiBuffer *instanceUbo = mInstanceUboRef.get();
    if (!instanceUbo) {
        qWarning("BasePass::uploadInstanceData - Instance UBO buffer object not created yet.");
        return;
    }
    if (instanceCount > 0 && instanceCount <= mMaxInstances) {
        quint32 dataSize = instanceCount * mInstanceBlockAlignedSize;
        if (dataSize > instanceUbo->size()) {
            qWarning(
                "BasePass::uploadInstanceData - Attempting to upload %u bytes, but instance UBO size is only %llu. Clamping instance count.",
                dataSize, instanceUbo->size());
            instanceCount = instanceUbo->size() / mInstanceBlockAlignedSize;
            if (instanceCount == 0) {
                qWarning("BasePass::uploadInstanceData - Instance UBO too small even for one instance.");
                return;
            }
            dataSize = instanceCount * mInstanceBlockAlignedSize;
        }

        if (instanceCount > 0) {
            batch->updateDynamicBuffer(instanceUbo, 0, dataSize, mInstanceDataBuffer.constData());
        }
    } else if (instanceCount > mMaxInstances) {
        qWarning(
            "BasePass::uploadInstanceData - instanceCount (%d) exceeds mMaxInstances (%d). This should have been caught earlier.",
            instanceCount, mMaxInstances);
    }
}

void BasePass::findActiveCamera() {
    if (!mWorld) {
        qWarning("BasePass::findActiveCamera - World is null.");
        mActiveCamera = INVALID_ENTITY;
        return;
    }
    mActiveCamera = INVALID_ENTITY;
    for (EntityID entity: mWorld->view<CameraComponent, TransformComponent>()) {
        // if (mWorld->hasComponent<ActiveCameraTag>(entity)) {
        mActiveCamera = entity;
        qInfo() << "BasePass: Found active camera entity:" << mActiveCamera;
        break;
        // }
    }

    if (mActiveCamera == INVALID_ENTITY) {
        qWarning("BasePass::findActiveCamera - No active camera entity found in the world!");
    }
}
