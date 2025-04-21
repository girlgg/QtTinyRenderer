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
    mRhi = builder.rhi();
    const QSize outputSize = builder.outputSize();

    // 声明输出资源
    mOutput.baseColor = builder.createTexture("BaseColor", outputSize, QRhiTexture::RGBA8, 1,
                                              QRhiTexture::RenderTarget | QRhiTexture::UsedAsTransferSource);
    mOutput.depthStencil = builder.createTexture("DepthStencil", outputSize, QRhiTexture::D24S8, 1,
                                                 QRhiTexture::RenderTarget | QRhiTexture::UsedAsTransferSource);

    // 声明 Uniform Buffers (暂存，每帧动态更新)
    mCameraUboRef = builder.createBuffer("CameraUBO", QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer,
                                         sizeof(CameraUniformBlock));
    mLightingUboRef = builder.createBuffer("LightingUBO", QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer,
                                           sizeof(LightingUniformBlock));

    // Instance UBO
    const quint32 alignment = mRhi->ubufAlignment();
    mInstanceBlockAlignedSize = sizeof(InstanceUniformBlock);
    if (mInstanceBlockAlignedSize % alignment != 0) {
        mInstanceBlockAlignedSize += alignment - (mInstanceBlockAlignedSize % alignment);
    }
    mInstanceUboRef = builder.createBuffer("InstanceUBO", QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer,
                                           mMaxInstances * mInstanceBlockAlignedSize);
    mInstanceDataBuffer.resize(mMaxInstances); // Resize CPU buffer

    // 设置 RenderTarget
    QRhiTextureRenderTargetDescription rtDesc(mOutput.baseColor.get());
    // rtDesc.setDepthStencilBuffer(mOutput.depthStencil.get());
    QSharedPointer<QRhiRenderTarget> renderTarget = builder.setupRenderTarget("BasePassRT", rtDesc);

    // 设置 Sampler
    // mDefaultSampler = mResourceManager->getDefaultSampler();
    // if (!mDefaultSampler) {
    //     qWarning("Default sampler not found in ResourceManager for BasePass.");
    //     mDefaultSampler.reset(mRhi->newSampler(QRhiSampler::Linear, QRhiSampler::Linear, QRhiSampler::None,
    //                                            QRhiSampler::Repeat, QRhiSampler::Repeat));
    //     if (mDefaultSampler)
    //         mDefaultSampler->create();
    // }

    createPipelines(builder, renderTarget->renderPassDescriptor());
}

void BasePass::execute(QRhiCommandBuffer *cmdBuffer) {
    if (!mWorld || !mResourceManager || !mPipeline || !mOutput.baseColor.isValid() || !mOutput.depthStencil.isValid() ||
        !mDefaultSampler) {
        qWarning("BasePass::execute prerequisites not met.");
        return;
    }

    // 1. Resource Updates (Uniforms, Instances, Persistent Resources)
    QRhiResourceUpdateBatch *batch = mRhi->nextResourceUpdateBatch(); // Get batch for this pass
    updateUniforms(batch);

    // Queue updates for persistent resources (meshes, materials, textures) needed this frame
    // This logic might need refinement. Perhaps ResourceManager provides a list of 'dirty' resources.
    // Or we iterate entities first to see what's needed.
    int currentInstanceIndex = 0;
    QHash<QString, QHash<QString, QVector<EntityID> > > entitiesByMeshThenMaterial;

    // --- Collect visible entities and prepare instance data ---
    for (EntityID entity: mWorld->view<RenderableComponent, MeshComponent, MaterialComponent, TransformComponent>()) {
        const auto *renderable = mWorld->getComponent<RenderableComponent>(entity);
        const auto *meshComp = mWorld->getComponent<MeshComponent>(entity);
        const auto *matComp = mWorld->getComponent<MaterialComponent>(entity);
        const auto *tfComp = mWorld->getComponent<TransformComponent>(entity);

        if (renderable && renderable->isVisible && meshComp && matComp && tfComp) {
            if (currentInstanceIndex >= mMaxInstances) {
                qWarning("BasePass::execute exceeded max instances (%d).", mMaxInstances);
                break; // Stop collecting if buffer full
            }

            QString meshId = meshComp->meshResourceId;
            QString matId = mResourceManager->generateMaterialCacheKey(matComp); // Assuming this exists

            // Check if resources are ready (or queue loading/update if not)
            // This interacts heavily with ResourceManager's state
            RhiMeshGpuData *meshGpu = mResourceManager->getMeshGpuData(meshId);
            RhiMaterialGpuData *matGpu = mResourceManager->getMaterialGpuData(matId);

            // Ensure material and its textures are loaded/updated if needed
            if (!matGpu) {
                mResourceManager->loadMaterial(matId, matComp); // Load definition if not present
                matGpu = mResourceManager->getMaterialGpuData(matId);
            }
            if (matGpu && !matGpu->ready) {
                mResourceManager->queueMaterialUpdate(matId, batch); // Queue texture uploads etc.
            }
            // Ensure mesh is loaded/updated if needed
            if (!meshGpu) {
                // Need mesh data definition to load it. Assume MeshComponent holds it or can find it.
                // Example: mResourceManager->loadMeshFromData(meshId, meshComp->vertices, meshComp->indices);
                // For now, assume it was loaded previously if getMeshGpuData returns nullptr
                qWarning("Mesh %s not found or loaded.", qPrintable(meshId));
                continue;
            }
            if (meshGpu && !meshGpu->ready) {
                // Need original vertex/index data to queue update
                // This implies ResourceManager might need to store or have access to it.
                // Example: mResourceManager->queueMeshUpdate(meshId, batch, vertices, indices);
                // Let's assume for now that BUILTIN meshes are handled elsewhere or always ready.
                if (meshId == BUILTIN_CUBE_MESH_ID) {
                    // Handle built-ins specifically if needed
                    mResourceManager->queueMeshUpdate(BUILTIN_CUBE_MESH_ID, batch, DEFAULT_CUBE_VERTICES,
                                                      DEFAULT_CUBE_INDICES);
                } else if (meshId == BUILTIN_PYRAMID_MESH_ID) {
                    mResourceManager->queueMeshUpdate(BUILTIN_PYRAMID_MESH_ID, batch, DEFAULT_PYRAMID_VERTICES,
                                                      DEFAULT_PYRAMID_INDICES);
                } else {
                    qWarning("Mesh %s is not ready, cannot queue update without source data.", qPrintable(meshId));
                    // Skip rendering this entity if mesh not ready and cannot update
                    continue;
                }
            }

            // If all resources seem okay (or update queued), add to draw list and instance buffer
            if (meshGpu && meshGpu->ready && matGpu && matGpu->ready) {
                // Check ready status *after* queueing updates
                entitiesByMeshThenMaterial[meshId][matId].append(entity);
                // Update instance data buffer (CPU side)
                mInstanceDataBuffer[currentInstanceIndex].model = tfComp->worldMatrix().toGenericMatrix<4, 4>();
                currentInstanceIndex++;
            }
        }
    }

    // Upload instance data gathered above
    uploadInstanceData(batch, currentInstanceIndex);

    // Submit all resource updates batched so far
    cmdBuffer->resourceUpdate(batch); // Submit the batch

    // 2. Begin Render Pass
    // Get the actual RenderTarget RHI object created by the builder
    // QRhiRenderTarget *renderTarget = mRhi->getRenderTarget(mOutput.baseColor.get());
    // Need helper in QRhi/Graph? No, get from resource.
    // if (!renderTarget) {
    // Fallback if direct access needed
    // auto *texRes = static_cast<RGTexture *>(mOutput.baseColor.mResource);
    // if (texRes) renderTarget = texRes->mRhiRenderTarget.get();
    // }

    // if (!renderTarget) {
    // qWarning("BasePass::execute failed to get RenderTarget.");
    // return;
    // }

    const QColor clearColor = QColor::fromRgbF(0.2f, 0.2f, 0.2f, 1.0f);
    const QRhiDepthStencilClearValue dsClearValue = {1.0f, 0};
    // cmdBuffer->beginPass(renderTarget, clearColor, dsClearValue, nullptr); // Pass nullptr for batch, already submitted

    // 3. Set Pipeline and Viewport/Scissor
    cmdBuffer->setGraphicsPipeline(mPipeline.get());
    // const QSize outputSize = renderTarget->pixelSize();
    // cmdBuffer->setViewport({0, 0, (float) outputSize.width(), (float) outputSize.height()});
    // cmdBuffer->setScissor({0, 0, outputSize.width(), outputSize.height()});

    // 4. Draw Entities (Instanced or Grouped)
    int drawnInstanceCount = 0; // Track offset into instance UBO

    for (auto meshIt = entitiesByMeshThenMaterial.constBegin(); meshIt != entitiesByMeshThenMaterial.constEnd(); ++
         meshIt) {
        const QString &meshId = meshIt.key();
        const QHash<QString, QVector<EntityID> > &materials = meshIt.value();
        RhiMeshGpuData *meshGpu = mResourceManager->getMeshGpuData(meshId);

        if (!meshGpu || !meshGpu->ready || meshGpu->indexCount == 0) {
            // Skip this mesh, count instances belonging to it
            for (const auto &matEntities: materials) { drawnInstanceCount += matEntities.size(); }
            continue;
        }

        // Set Vertex/Index Buffers for this mesh
        QRhiCommandBuffer::VertexInput vtxBinding(meshGpu->vertexBuffer.get(), 0);
        cmdBuffer->setVertexInput(0, 1, &vtxBinding, meshGpu->indexBuffer.get(), 0, QRhiCommandBuffer::IndexUInt16);

        for (auto matIt = materials.constBegin(); matIt != materials.constEnd(); ++matIt) {
            const QString &materialId = matIt.key();
            const QVector<EntityID> &entities = matIt.value();
            RhiMaterialGpuData *matGpu = mResourceManager->getMaterialGpuData(materialId);

            if (!matGpu || !matGpu->ready) {
                drawnInstanceCount += entities.size(); // Account for skipped instances
                continue;
            }

            // Get the primary texture (e.g., Albedo) for this material
            // Adapt this if materials use multiple textures
            RhiTextureGpuData *texGpu = mResourceManager->getTextureGpuData(matGpu->albedoId);
            if (!texGpu || !texGpu->ready || !texGpu->texture) {
                // Use default texture if main one isn't ready
                texGpu = mResourceManager->getTextureGpuData(DEFAULT_WHITE_TEXTURE_ID);
                if (!texGpu || !texGpu->ready) {
                    qWarning("Material %s texture %s not ready, and default white texture missing/not ready.",
                             qPrintable(materialId), qPrintable(matGpu->albedoId));
                    drawnInstanceCount += entities.size();
                    continue;
                }
            }

            // Create and set ShaderResourceBindings for this batch of instances
            // Use the layout created in setup()
            QScopedPointer<QRhiShaderResourceBindings> drawSrb(mRhi->newShaderResourceBindings());
            // Define the actual bindings for this draw call
            drawSrb->setBindings({
                QRhiShaderResourceBinding::uniformBuffer(
                    0, QRhiShaderResourceBinding::VertexStage | QRhiShaderResourceBinding::FragmentStage,
                    mCameraUboRef.get()),
                QRhiShaderResourceBinding::uniformBuffer(1, QRhiShaderResourceBinding::FragmentStage,
                                                         mLightingUboRef.get()),
                QRhiShaderResourceBinding::sampledTexture(2, QRhiShaderResourceBinding::FragmentStage,
                                                          texGpu->texture.get(), mDefaultSampler.get()),
                // Use actual texture
                QRhiShaderResourceBinding::uniformBuffer(3, QRhiShaderResourceBinding::VertexStage,
                                                         mInstanceUboRef.get(),
                                                         drawnInstanceCount * mInstanceBlockAlignedSize,
                                                         mInstanceBlockAlignedSize) // Dynamic offset for first instance
            });
            if (!drawSrb->create()) {
                qWarning("Failed to create draw SRB for mesh %s, material %s.", qPrintable(meshId),
                         qPrintable(materialId));
                drawnInstanceCount += entities.size();
                continue;
            }

            cmdBuffer->setShaderResources(drawSrb.get());

            // Draw all entities using this mesh/material combination
            // If hardware instancing is desired, use drawIndexedInstanced
            cmdBuffer->drawIndexed(meshGpu->indexCount, entities.size()); // Draw 'N' instances

            drawnInstanceCount += entities.size(); // Advance instance offset
        }
    }

    // 5. End Render Pass
    cmdBuffer->endPass();

    // mFrameDrawSrbs.clear(); // Clear temporary SRBs if used
}

void BasePass::createPipelines(RGBuilder &builder, QRhiRenderPassDescriptor *rpDesc) {
    const quint32 vertexStride = sizeof(VertexData);
    QRhiVertexInputLayout inputLayout;
    inputLayout.setBindings({QRhiVertexInputBinding(vertexStride)});
    inputLayout.setAttributes({
        QRhiVertexInputAttribute(0, 0, QRhiVertexInputAttribute::Float3, offsetof(VertexData, position)),
        QRhiVertexInputAttribute(0, 1, QRhiVertexInputAttribute::Float3, offsetof(VertexData, normal)),
        QRhiVertexInputAttribute(0, 2, QRhiVertexInputAttribute::Float2, offsetof(VertexData, texCoord))
    });

    // --- Shaders ---
    ShaderBundle::getInstance()->loadShader("Shaders/baseGeo", {
                                                {":/shaders/baseGeo.vert.qsb", QRhiShaderStage::Vertex},
                                                {":/shaders/baseGeo.frag.qsb", QRhiShaderStage::Fragment}
                                            });
    QRhiShaderStage vs = ShaderBundle::getInstance()->getShaderStage("Shaders/baseGeo", QRhiShaderStage::Vertex);
    QRhiShaderStage fs = ShaderBundle::getInstance()->getShaderStage("Shaders/baseGeo", QRhiShaderStage::Fragment);
    // if (!vs.isValid() || !fs.isValid()) {
    // qFatal("Failed to load base shaders for BasePass.");
    // return;
    // }

    // --- Shader 资源绑定 Layout ---
    QVector<QRhiShaderResourceBinding> bindingsLayout = {
        QRhiShaderResourceBinding::uniformBuffer(
            0, QRhiShaderResourceBinding::VertexStage | QRhiShaderResourceBinding::FragmentStage, nullptr),
        QRhiShaderResourceBinding::uniformBuffer(1, QRhiShaderResourceBinding::FragmentStage, nullptr),
        QRhiShaderResourceBinding::sampledTexture(2, QRhiShaderResourceBinding::FragmentStage, nullptr, nullptr),
        QRhiShaderResourceBinding::uniformBuffer(3, QRhiShaderResourceBinding::VertexStage, nullptr, 0,
                                                 mInstanceBlockAlignedSize)
    };
    mBaseSrbLayout = builder.setupShaderResourceBindings("BasePipelineSRBLayout", bindingsLayout);

    // --- Graphics Pipeline ---
    QSharedPointer<QRhiGraphicsPipeline> pipelineState;
    pipelineState.reset(mRhi->newGraphicsPipeline());
    pipelineState->setName(QByteArrayLiteral("Base Opaque Pipeline"));

    // Reference the SRB layout
    pipelineState->setShaderResourceBindings(mBaseSrbLayout.get());
    pipelineState->setShaderStages({vs, fs});
    pipelineState->setVertexInputLayout(inputLayout);
    pipelineState->setRenderPassDescriptor(rpDesc);
    pipelineState->setSampleCount(mOutput.baseColor.get() ? mOutput.baseColor.get()->sampleCount() : 1);

    pipelineState->setCullMode(QRhiGraphicsPipeline::Back);
    pipelineState->setFrontFace(QRhiGraphicsPipeline::CCW);
    pipelineState->setDepthTest(true);
    pipelineState->setDepthWrite(true);
    pipelineState->setDepthOp(QRhiGraphicsPipeline::Less);

    mPipeline = builder.setupGraphicsPipeline("BasePipeline", pipelineState, rpDesc);
}

void BasePass::updateUniforms(QRhiResourceUpdateBatch *batch) {
    if (!mWorld || !mCameraUboRef.isValid() || !mLightingUboRef.isValid()) return;

    // --- Update Camera UBO ---
    CameraUniformBlock camData;
    bool cameraOk = false;
    if (mActiveCamera == INVALID_ENTITY) findActiveCamera(); // Find camera if not already found

    if (mActiveCamera != INVALID_ENTITY) {
        // ... (Same logic as in RasterizeRenderSystem::submitResourceUpdates to fill camData) ...
        // Fetch CameraComponent and TransformComponent, calculate matrices
        auto *camComp = mWorld->getComponent<CameraComponent>(mActiveCamera);
        auto *camTf = mWorld->getComponent<TransformComponent>(mActiveCamera);
        if (camComp && camTf) {
            // ... calculate view and projection matrices ...
            QMatrix4x4 viewMatrix;
            viewMatrix.lookAt(camTf->position(), camTf->position() + camTf->rotation().rotatedVector({0, 0, -1}),
                              camTf->rotation().rotatedVector({0, 1, 0}));

            QMatrix4x4 projMatrix;
            const QSize outputPixelSize = mOutput.baseColor.pixelSize(); // Use graph output size
            float aspect = outputPixelSize.width() / (float) qMax(1, outputPixelSize.height());
            projMatrix.perspective(camComp->mFov, aspect, camComp->mNearPlane, camComp->mFarPlane);
            projMatrix *= mRhi->clipSpaceCorrMatrix(); // Apply correction matrix

            camData.view = viewMatrix.toGenericMatrix<4, 4>();
            camData.projection = projMatrix.toGenericMatrix<4, 4>();
            camData.viewPos = camTf->position();
            cameraOk = true;
        }
    }
    if (!cameraOk) {
        // Fallback
        QMatrix4x4 identity;
        camData.view = identity.toGenericMatrix<4, 4>();
        camData.projection = identity.toGenericMatrix<4, 4>();
        camData.viewPos = QVector3D(0, 0, 5); // Default position
    }
    batch->updateDynamicBuffer(mCameraUboRef.get(), 0, sizeof(CameraUniformBlock), &camData);

    // --- Update Lighting UBO ---
    LightingUniformBlock lightData;
    memset(&lightData, 0, sizeof(LightingUniformBlock));
    int pointLightIndex = 0;

    for (EntityID entity: mWorld->view<LightComponent, TransformComponent>()) {
        auto *lightComp = mWorld->getComponent<LightComponent>(entity);
        auto *lightTf = mWorld->getComponent<TransformComponent>(entity);
        if (!lightComp || !lightTf) continue;
        switch (lightComp->type) {
            case LightType::Directional:
                if (!lightData.dirLight.enabled) {
                    // Only support one directional light for now
                    lightData.dirLight.direction = lightTf->rotation().rotatedVector({0, 0, -1}).normalized();
                    lightData.dirLight.ambient = lightComp->ambient * lightComp->intensity;
                    lightData.dirLight.diffuse = lightComp->color * lightComp->intensity;
                    lightData.dirLight.specular = lightComp->specular * lightComp->intensity;
                    // Assuming specular color is white modulated by intensity
                    lightData.dirLight.enabled = 1; // Use 1 for true in shader
                }
                break;
            case LightType::Point:
                if (pointLightIndex < MAX_POINT_LIGHTS) {
                    auto &pl = lightData.pointLights[pointLightIndex];
                    pl.position = lightTf->position();
                    // Pack color and intensity together if shader expects vec4(rgb, intensity)
                    // pl.colorAndIntensity = QVector4D(lightComp->color.x(), lightComp->color.y(), lightComp->color.z(),
                    // lightComp->intensity);
                    pl.attenuation = QVector3D(lightComp->constantAttenuation,
                                               lightComp->linearAttenuation,
                                               lightComp->quadraticAttenuation);
                    // pl.enabled = 1;
                    pointLightIndex++;
                }
                break;
            // Add SpotLight case if needed
        }
    }
    lightData.numPointLights = pointLightIndex;
    batch->updateDynamicBuffer(mLightingUboRef.get(), 0, sizeof(LightingUniformBlock), &lightData);
}

void BasePass::uploadInstanceData(QRhiResourceUpdateBatch *batch, int instanceCount) {
    if (instanceCount > 0 && mInstanceUboRef.isValid()) {
        batch->updateDynamicBuffer(mInstanceUboRef.get(), 0,
                                   instanceCount * mInstanceBlockAlignedSize,
                                   mInstanceDataBuffer.data());
    }
}

void BasePass::findActiveCamera() {
    if (!mWorld) {
        return;
    }
    mActiveCamera = INVALID_ENTITY;
    for (EntityID entity: mWorld->view<CameraComponent, TransformComponent/*, ActiveCameraTag*/>()) {
        mActiveCamera = entity;
        break;
    }
    if (mActiveCamera == INVALID_ENTITY) {
        qWarning() << "No active camera found in the world!";
    }
}
