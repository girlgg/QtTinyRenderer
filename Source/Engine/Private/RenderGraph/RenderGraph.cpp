#include "RenderGraph/RenderGraph.h"

#include "RenderGraph/RGBuilder.h"
#include "RenderGraph/RGPass.h"
#include "RenderGraph/RGResource.h"

RenderGraph::RenderGraph(QRhi *rhi, QSharedPointer<ResourceManager> resManager, QSharedPointer<World> world,
                         const QSize &outputSize, QRhiRenderPassDescriptor *swapChainRpDesc)
    : mRhi(rhi), mResourceManager(resManager), mWorld(world), mOutputSize(outputSize),
      mSwapChainRpDesc(swapChainRpDesc), mCompiled(false) {
    qInfo() << "RenderGraph created. Initial size:" << outputSize;
    // --- Assert prerequisites ---
    Q_ASSERT_X(mRhi != nullptr, "RenderGraph::RenderGraph", "QRhi pointer cannot be null.");
    Q_ASSERT_X(mResourceManager != nullptr, "RenderGraph::RenderGraph", "ResourceManager pointer cannot be null.");
    Q_ASSERT_X(mWorld != nullptr, "RenderGraph::RenderGraph", "World pointer cannot be null.");
    Q_ASSERT_X(mSwapChainRpDesc != nullptr, "RenderGraph::RenderGraph",
               "SwapChain RenderPassDescriptor pointer cannot be null.");
    // --- 注册 SwapChain RenderTarget 代理 ---
    if (mSwapChainRpDesc) {
        auto swapChainRTProxyDesc = QSharedPointer<RGRenderTarget>::create(
            "SwapChainRenderTargetProxy", mSwapChainRpDesc);
        swapChainRTProxyDesc->mIsExternalRpDesc = true;
        registerResource(swapChainRTProxyDesc);
        qInfo() << "Registered SwapChainRenderTargetProxy with external RPDesc.";
    } else {
        qCritical("RenderGraph created with null SwapChainRpDesc! PresentPass pipeline setup will likely fail.");
    }
}

RenderGraph::~RenderGraph() {
    mPasses.clear();
    mResources.clear();
    mExecutionOrder.clear();
}

void RenderGraph::compile() {
    qInfo() << "RenderGraph::compile started...";
    if (!mOutputSize.isValid() || mOutputSize.width() <= 0 || mOutputSize.height() <= 0) {
        qCritical("RenderGraph::compile - Cannot compile with invalid output size: %dx%d. Compile aborted.",
                  mOutputSize.width(), mOutputSize.height());
        mCompiled = false; // Ensure compiled flag is false
        return;
    }
    // --- 设置所有 Pass ---
    // TODO 拓扑排序
    qInfo() << "  Running setup for" << mPasses.size() << "passes...";
    mExecutionOrder.clear();
    for (const auto &pass: qAsConst(mPasses)) {
        if (pass) {
            qInfo() << "    - Setting up pass:" << pass->name();
            RGBuilder passBuilder(this, pass.get());
            pass->setup(passBuilder);
            mExecutionOrder.append(pass.get());
        } else {
            qWarning("  Found null pass pointer during setup.");
        }
    }
    qInfo() << "  Pass setup complete. Execution order size:" << mExecutionOrder.size();

    // --- 创建 RHI 资源 ---
    qInfo() << "  Creating/Updating RHI resources for" << mResources.count() << "registered items...";

    // 阶段1：基本资源 (Textures, Buffers, Samplers, SRB Layouts)
    qInfo() << "    Phase 1: Basic Resources...";
    for (auto it = mResources.begin(); it != mResources.end(); ++it) {
        RGResource *res = it.value().get();
        switch (res->type()) {
            case RGResource::Type::Texture:
            case RGResource::Type::Buffer:
            case RGResource::Type::RenderBuffer:
            case RGResource::Type::Sampler:
            case RGResource::Type::ShaderResourceBindings:
                createRhiResource(res);
                break;
            default:
                break;
        }
    }
    // 阶段2：Render Targets
    qInfo() << "    Phase 2: Render Targets...";
    for (auto it = mResources.begin(); it != mResources.end(); ++it) {
        RGResource *res = it.value().get();
        if (res->type() == RGResource::Type::RenderTarget) {
            if (res->name() != "SwapChainRenderTargetProxy") {
                createRhiResource(res);
            }
        }
    }

    // 阶段3：创建管线
    for (auto it = mResources.begin(); it != mResources.end(); ++it) {
        RGResource *res = it.value().get();
        if (res && res->type() == RGResource::Type::Pipeline) {
            createRhiResource(res);
        }
    }

    int createdCount = 0;
    int failedCount = 0;
    for (auto it = mResources.begin(); it != mResources.end(); ++it) {
        RGResource *res = it.value().get();
        if (!res) continue;
        bool rhiObjectCreated = false;
        if (res->name() == "SwapChainRenderTargetProxy") {
            rhiObjectCreated = true;
        } else {
            rhiObjectCreated = (res->mRhiTexture || res->mRhiBuffer || res->mRhiRenderBuffer ||
                                res->mRhiRenderTarget || res->mRhiGraphicsPipeline ||
                                res->mRhiShaderResourceBindings || res->mRhiSampler);
        }
        bool created = (res->mRhiTexture || res->mRhiBuffer || res->mRhiRenderBuffer || res->mRhiRenderTarget || res->
                        mRhiGraphicsPipeline || res->mRhiShaderResourceBindings || res->mRhiSampler);
        if (rhiObjectCreated) {
            createdCount++;
        } else {
            if (res->name() != "SwapChainRenderTargetProxy") {
                qWarning("    - Failed to create/find RHI object for resource '%s' (Type: %d) after compile.",
                         qPrintable(res->name()), static_cast<int>(res->type()));
                failedCount++;
            }
        }
    }
    qInfo() << "RenderGraph::compile - Finished creating RHI resources. Created:" << createdCount << "Failed:" <<
            failedCount;

    if (failedCount > 0) {
        qCritical(
            "RenderGraph::compile - CRITICAL: %d required RHI resources could not be created. Rendering will likely fail or be incorrect.",
            failedCount);
    } else {
        qInfo() << "RenderGraph::compile finished successfully.";
        mCompiled = true;
    }
}

void RenderGraph::execute(QRhiSwapChain *swapChain) {
    if (!mCompiled) {
        qWarning("RenderGraph::execute called before successful compile(). Skipping execution.");
        return;
    }
    if (!mRhi) {
        qCritical("RenderGraph::execute - QRhi instance is null. Cannot execute.");
        return;
    }
    if (!swapChain) {
        qWarning("RenderGraph::execute called with null swapChain pointer.");
        return;
    }
    if (!mCommandBuffer) {
        qWarning("RenderGraph::execute called without a valid command buffer.");
        return;
    }
    if (mExecutionOrder.isEmpty()) {
        qWarning("RenderGraph::execute called but mExecutionOrder is empty. Was compile() called after adding passes?");
        if (mPasses.isEmpty()) {
            qWarning(" -> RenderGraph::execute: mPasses is also empty. No passes were added to the graph.");
        } else {
            qWarning(
                " -> RenderGraph::execute: mPasses contains %d passes, but mExecutionOrder is empty. Check compile() logic.",
                mPasses.size());
        }
        return;
    }
    // --- 设置帧状态 ---
    mCurrentSwapChain = swapChain;
    qInfo() << "RenderGraph::execute - Executing" << mExecutionOrder.size() << "passes...";
    // TODO: pass之间插入屏障
    for (RGPass *pass: std::as_const(mExecutionOrder)) {
        if (pass) {
            pass->execute(mCommandBuffer);
        } else {
            qWarning("RenderGraph::execute - Found null pass pointer in execution order.");
        }
    }
    // TODO: 插入结束屏障
    // TODO: 资源释放
    mCurrentSwapChain = nullptr;
    qInfo() << "RenderGraph::execute - Finished executing passes.";
}

QSharedPointer<RGResource> RenderGraph::findResource(const QString &name) const {
    return mResources.value(name, nullptr);
}

bool RenderGraph::removeResource(const QString &name) {
    return mResources.remove(name) > 0;
}

RGResource *RenderGraph::registerResource(const QSharedPointer<RGResource> &resource) {
    if (!resource) {
        qWarning("RenderGraph::registerResource: Attempted to register a null resource pointer.");
        return nullptr;
    }
    if (resource->name().isEmpty()) {
        qWarning("RenderGraph::registerResource: Attempted to register a resource with an empty name.");
        return nullptr;
    }

    const QString &resourceName = resource->name();
    if (mResources.contains(resourceName)) {
        if (mResources.value(resourceName) != resource) {
            qWarning(
                "RenderGraph::registerResource: Resource name '%s' already exists but with a DIFFERENT instance. Overwriting with new instance.",
                qPrintable(resourceName));
        } else {
            return resource.get();
        }
    }
    mResources.insert(resourceName, resource);
    return resource.get();
}

void RenderGraph::setCommandBuffer(QRhiCommandBuffer *cmdBuffer) {
    mCommandBuffer = cmdBuffer;
}

void RenderGraph::setOutputSize(const QSize &size) {
    if (mOutputSize != size) {
        qInfo() << "RenderGraph output size changed from" << mOutputSize << "to" << size;
        mOutputSize = size;
        mCompiled = false;

        // --- 更新资源 ---
        for (auto it = mResources.begin(); it != mResources.end(); ++it) {
            RGResource *res = it.value().get();
            if (!res) continue;

            if (auto texRes = dynamic_cast<RGTexture *>(res)) {
                if (res->name() == "BaseColor") {
                    if (texRes->size() != mOutputSize) {
                        qInfo() << "  Updating size description for Texture:" << res->name();
                    }
                }
            } else if (auto rbRes = dynamic_cast<RGRenderBuffer *>(res)) {
                if (res->name() == "DepthStencil") {
                    if (rbRes->size() != mOutputSize) {
                        qInfo() << "  Updating size description for RenderBuffer:" << res->name();
                    }
                }
            }
        }
    }
}

QRhiCommandBuffer *RenderGraph::getCommandBuffer() const {
    return mCommandBuffer;
}

void RenderGraph::createRhiResource(RGResource *resource) {
    if (!resource || !mRhi) {
        qWarning("createRhiResource: Null resource or RHI pointer.");
        return;
    }
    // --- 检查是否资源更新 ---
    bool needsRebuild = false;
    if (resource->mRhiTexture &&
        resource->mRhiTexture->pixelSize() != static_cast<RGTexture *>(resource)->size()) {
        needsRebuild = true;
    }
    if (resource->mRhiRenderBuffer &&
        resource->mRhiRenderBuffer->pixelSize() != static_cast<RGRenderBuffer *>(resource)->size()) {
        needsRebuild = true;
    }

    // --- 构建 RHI 资源 ---
    bool exists = (resource->mRhiTexture || resource->mRhiBuffer || resource->mRhiRenderBuffer ||
                   resource->mRhiRenderTarget || resource->mRhiGraphicsPipeline ||
                   resource->mRhiShaderResourceBindings || resource->mRhiSampler);

    if (!exists || needsRebuild) {
        if (needsRebuild) {
            qInfo() << "  Rebuilding RHI object for resource:" << resource->name();
            if (resource->mRhiTexture) resource->mRhiTexture.reset();
            if (resource->mRhiBuffer) resource->mRhiBuffer.reset();
            if (resource->mRhiRenderBuffer) resource->mRhiRenderBuffer.reset();
            if (resource->mRhiRenderTarget) resource->mRhiRenderTarget.reset();
            if (resource->mRhiGraphicsPipeline) resource->mRhiGraphicsPipeline.reset();
            if (resource->mRhiShaderResourceBindings) resource->mRhiShaderResourceBindings.reset();
            if (resource->mRhiSampler) resource->mRhiSampler.reset();
        } else {
            qInfo() << "  Creating RHI object for resource:" << resource->name();
        }

        bool success = resource->build(mRhi);

        if (!success) {
            qWarning("  --> FAILED to create RHI object or its dependencies for: %s (Type: %d)",
                     qPrintable(resource->name()), static_cast<int>(resource->type()));
        } else {
            qInfo() << "  --> Successfully created RHI object for:" << resource->name();
        }
    } else {
        qInfo() << "  RHI object for resource '" << resource->name() << "' already exists and doesn't need rebuild.";
    }
}

void RenderGraph::releaseRhiResource(RGResource *resource) {
    if (!resource) return;
    qInfo() << "RenderGraph: Releasing RHI resource for" << resource->name();
    resource->mRhiTexture.reset();
    resource->mRhiBuffer.reset();
    resource->mRhiSampler.reset();
    resource->mRhiRenderBuffer.reset();
    resource->mRhiRenderTarget.reset();
    resource->mRhiGraphicsPipeline.reset();
    resource->mRhiShaderResourceBindings.reset();
}
