#include "RenderGraph/RGBuilder.h"

#include "RenderGraph/RenderGraph.h"
#include "RenderGraph/RGPass.h"
#include "RenderGraph/RGResource.h"
#include "Resources/ResourceManager.h"

RGBuilder::RGBuilder(RenderGraph *rg, RGPass *currentPass)
    : mGraph(rg), mCurrentPass(currentPass) {
    Q_ASSERT_X(mGraph != nullptr, "RGBuilder::RGBuilder", "RenderGraph pointer cannot be null.");
    Q_ASSERT_X(mCurrentPass != nullptr, "RGBuilder::RGBuilder", "CurrentPass pointer cannot be null.");
}

RGTextureRef RGBuilder::createTexture(const QString &name, const QSize &size, QRhiTexture::Format format,
                                      int sampleCount, QRhiTexture::Flags flags) {
    // 确保Graph名字唯一
    QSharedPointer<RGResource> existingRes = mGraph->findResource(name);
    if (existingRes) {
        if (auto texRes = qSharedPointerCast<RGTexture>(existingRes)) {
            if (texRes->size() != size || texRes->format() != format || texRes->sampleCount() != sampleCount || texRes->
                flags() != flags) {
                qWarning(
                    "RGBuilder::createTexture: Resource '%s' exists but with DIFFERENT parameters. Returning existing. Potential issues may arise.",
                    qPrintable(name));
            } else {
                qDebug("RGBuilder::createTexture: Resource '%s' already exists with same parameters.",
                       qPrintable(name));
            }
            return RGTextureRef(texRes);
        }
        qCritical("RGBuilder::createTexture: Resource name '%s' already exists but is NOT a Texture (Type: %d)!",
                  qPrintable(name), static_cast<int>(existingRes->type()));
        return RGTextureRef();
    }
    QSharedPointer<RGTexture> texResource = QSharedPointer<RGTexture>::create(name, size, format, sampleCount, flags);
    mGraph->registerResource(texResource);
    return RGTextureRef(texResource);
}

RGBufferRef RGBuilder::createBuffer(const QString &name, QRhiBuffer::Type type, QRhiBuffer::UsageFlags usage,
                                    quint32 size) {
    QSharedPointer<RGResource> existingRes = mGraph->findResource(name);
    if (existingRes) {
        if (auto bufRes = qSharedPointerCast<RGBuffer>(existingRes)) {
            if (bufRes->bufType() != type || bufRes->usage() != usage || bufRes->size() != size) {
                qWarning(
                    "RGBuilder::createBuffer: Resource '%s' exists but with DIFFERENT parameters. Returning existing.",
                    qPrintable(name));
            }
            return RGBufferRef(bufRes);
        }
        qCritical("RGBuilder::createBuffer: Resource name '%s' already exists but is NOT a Buffer (Type: %d)!",
                  qPrintable(name), static_cast<int>(existingRes->type()));
        return RGBufferRef();
    }
    qDebug() << "RGBuilder::createBuffer: Creating new description for '" << name << "'";
    auto bufResource = QSharedPointer<RGBuffer>::create(name, type, usage, size);
    mGraph->registerResource(bufResource);
    return RGBufferRef(bufResource);
}

RGSamplerRef RGBuilder::setupSampler(const QString &name,
                                     QRhiSampler::Filter magFilter,
                                     QRhiSampler::Filter minFilter,
                                     QRhiSampler::Filter mipmapMode,
                                     QRhiSampler::AddressMode addressU,
                                     QRhiSampler::AddressMode addressV,
                                     QRhiSampler::AddressMode addressW) {
    QSharedPointer<RGResource> existingRes = mGraph->findResource(name);
    if (existingRes) {
        if (auto samplerRes = qSharedPointerCast<RGSampler>(existingRes)) {
            if (samplerRes->magFilter() != magFilter || samplerRes->minFilter() != minFilter || samplerRes->mipmapMode()
                != mipmapMode ||
                samplerRes->addressU() != addressU || samplerRes->addressV() != addressV || samplerRes->addressW() !=
                addressW) {
                qWarning(
                    "RGBuilder::setupSampler: Resource '%s' exists but with DIFFERENT parameters. Returning existing.",
                    qPrintable(name));
            }
            return RGSamplerRef(samplerRes);
        }
        qCritical("RGBuilder::setupSampler: Resource name '%s' already exists but is NOT a Sampler (Type: %d)!",
                  qPrintable(name), static_cast<int>(existingRes->type()));
        return RGSamplerRef();
    }

    auto samplerResource = QSharedPointer<RGSampler>::create(name, magFilter, minFilter, mipmapMode, addressU, addressV,
                                                             addressW);
    mGraph->registerResource(samplerResource);
    return RGSamplerRef(samplerResource);
}

RGRenderBufferRef RGBuilder::setupRenderBuffer(const QString &name, QRhiRenderBuffer::Type type, const QSize &size,
                                               int sampleCount, QRhiRenderBuffer::Flags flags) {
    QSharedPointer<RGResource> existingRes = mGraph->findResource(name);
    if (existingRes) {
        if (auto rbRes = qSharedPointerCast<RGRenderBuffer>(existingRes)) {
            if (rbRes->rbType() != type || rbRes->size() != size || rbRes->sampleCount() != sampleCount || rbRes->
                flags() != flags) {
                qWarning(
                    "RGBuilder::setupRenderBuffer: Resource '%s' exists but with DIFFERENT parameters. Returning existing.",
                    qPrintable(name));
            }
            return RGRenderBufferRef(rbRes);
        }
        qCritical(
            "RGBuilder::setupRenderBuffer: Resource name '%s' already exists but is NOT a RenderBuffer (Type: %d)!",
            qPrintable(name), static_cast<int>(existingRes->type()));
        return RGRenderBufferRef();
    }
    auto rbResource = QSharedPointer<RGRenderBuffer>::create(name, type, size, sampleCount, flags);
    mGraph->registerResource(rbResource);
    return RGRenderBufferRef(rbResource);
}

RGRenderTargetRef RGBuilder::setupRenderTarget(const QString &name, const QVector<RGTextureRef> &colorRefs,
                                               RGResourceRef dsRef) {
    QSharedPointer<RGResource> existingRes = mGraph->findResource(name);
    if (existingRes) {
        if (auto rtRes = qSharedPointerCast<RGRenderTarget>(existingRes)) {
            qWarning("RGBuilder::setupRenderTarget: Resource '%s' already exists. Returning existing.",
                     qPrintable(name));
            return RGRenderTargetRef(rtRes);
        }
        qCritical(
            "RGBuilder::setupRenderTarget: Resource name '%s' already exists but is NOT a RenderTarget (Type: %d)!",
            qPrintable(name), static_cast<int>(existingRes->type()));
        return RGRenderTargetRef();
    }
    for (const auto &colorRef: colorRefs) {
        if (!colorRef.isValid()) {
            qWarning("RGBuilder::setupRenderTarget '%s': Provided color attachment Ref is invalid.", qPrintable(name));
            return RGRenderTargetRef();
        }
    }
    if (dsRef.isValid() && dsRef.mResource->type() != RGResource::Type::RenderBuffer) {
        qCritical("Depth attachment must be a RenderBuffer");
        return RGRenderTargetRef();
    }
    auto rtResource = QSharedPointer<RGRenderTarget>::create(name, colorRefs, dsRef);
    mGraph->registerResource(rtResource);
    return RGRenderTargetRef(rtResource);
}

RGRenderTargetRef RGBuilder::getRenderTarget(const QString &name) {
    QSharedPointer<RGResource> res = mGraph->findResource(name);
    if (res) {
        if (auto rtRes = qSharedPointerCast<RGRenderTarget>(res)) {
            return RGRenderTargetRef(rtRes);
        }
        qWarning("RGBuilder::getRenderTarget: Resource '%s' found but is not a RenderTarget (Type: %d).",
                 qPrintable(name), static_cast<int>(res->type()));
    } else {
        qWarning("RGBuilder::getRenderTarget: Resource '%s' not found in the graph.", qPrintable(name));
    }
    return RGRenderTargetRef();
}

RGShaderResourceBindingsRef RGBuilder::setupShaderResourceBindings(const QString &name,
                                                                   const QVector<QRhiShaderResourceBinding> &bindings) {
    QSharedPointer<RGResource> existingRes = mGraph->findResource(name);
    if (existingRes) {
        if (auto srbRes = qSharedPointerCast<RGShaderResourceBindings>(existingRes)) {
            qWarning("RGBuilder::setupShaderResourceBindings: Resource '%s' already exists. Returning existing.",
                     qPrintable(name));
            return RGShaderResourceBindingsRef(srbRes);
        }
        qCritical(
            "RGBuilder::setupShaderResourceBindings: Resource name '%s' already exists but is NOT ShaderResourceBindings (Type: %d)!",
            qPrintable(name), static_cast<int>(existingRes->type()));
        return RGShaderResourceBindingsRef();
    }
    auto srbResource = QSharedPointer<RGShaderResourceBindings>::create(name, bindings);
    mGraph->registerResource(srbResource);
    return RGShaderResourceBindingsRef(srbResource);
}

RGTextureRef RGBuilder::writeTexture(const QString &name, const QSize &size, QRhiTexture::Format format,
                                     int sampleCount, QRhiTexture::Flags flags) {
    qDebug() << "RGBuilder: Pass" << mCurrentPass->name() << "writes Texture" << name;
    return createTexture(name, size, format, sampleCount, flags);
}

RGTextureRef RGBuilder::readTexture(const QString &name) {
    qDebug() << "RGBuilder: Pass" << mCurrentPass->name() << "reads Texture" << name;
    QSharedPointer<RGResource> res = mGraph->findResource(name);
    if (!res) {
        qWarning("RGBuilder::readTexture - Resource '%s' not found in the graph. Pass '%s' cannot read it.",
                 qPrintable(name), qPrintable(mCurrentPass->name()));
        return RGTextureRef();
    }
    if (auto texRes = qSharedPointerCast<RGTexture>(res)) {
        return RGTextureRef(texRes);
    }
    qWarning("RGBuilder::readTexture - Resource '%s' is not a Texture (Type: %d). Pass '%s' cannot read it.",
             qPrintable(name), static_cast<int>(res->type()), qPrintable(mCurrentPass->name()));
    return RGTextureRef();
}

RGRenderBufferRef RGBuilder::writeDepthStencil(const QString &name, const QSize &size, int sampleCount) {
    qDebug() << "RGBuilder: Pass" << mCurrentPass->name() << "writes DepthStencil" << name;
    return setupRenderBuffer(name, QRhiRenderBuffer::DepthStencil, size, sampleCount);
}

RGRenderBufferRef RGBuilder::readDepthStencil(const QString &name) {
    qDebug() << "RGBuilder: Pass" << mCurrentPass->name() << "reads DepthStencil" << name;
    QSharedPointer<RGResource> res = mGraph->findResource(name);
    if (!res) {
        qWarning("RGBuilder::readDepthStencil - Resource '%s' not found. Pass '%s' cannot read it.", qPrintable(name),
                 qPrintable(mCurrentPass->name()));
        return RGRenderBufferRef();
    }
    if (auto rbRes = qSharedPointerCast<RGRenderBuffer>(res)) {
        if (rbRes->rbType() == QRhiRenderBuffer::DepthStencil) {
            return RGRenderBufferRef(rbRes);
        }
        qWarning(
            "RGBuilder::readDepthStencil - Resource '%s' is a RenderBuffer but not DepthStencil type. Pass '%s' cannot read it.",
            qPrintable(name), qPrintable(mCurrentPass->name()));
        return RGRenderBufferRef();
    }
    qWarning(
        "RGBuilder::readDepthStencil - Resource '%s' is not a RenderBuffer (Type: %d). Pass '%s' cannot read it.",
        qPrintable(name), static_cast<int>(res->type()), qPrintable(mCurrentPass->name()));
    return RGRenderBufferRef();
}

RGPipelineRef RGBuilder::setupGraphicsPipeline(const QString &name,
                                               RGShaderResourceBindingsRef srbLayoutRef,
                                               RGRenderTargetRef renderTargetRef,
                                               const QVector<QRhiShaderStage> &shaderStages,
                                               const QRhiVertexInputLayout &vertexInputLayout,
                                               QRhiGraphicsPipeline::Topology topology,
                                               QRhiGraphicsPipeline::CullMode cullMode,
                                               QRhiGraphicsPipeline::FrontFace frontFace, bool depthTest,
                                               bool depthWrite,
                                               QRhiGraphicsPipeline::CompareOp depthOp, bool stencilTest,
                                               QRhiGraphicsPipeline::StencilOpState stencilFront,
                                               QRhiGraphicsPipeline::StencilOpState stencilBack,
                                               quint32 stencilReadMask, quint32 stencilWriteMask,
                                               const QVector<QRhiGraphicsPipeline::TargetBlend> &targetBlends,
                                               QRhiGraphicsPipeline::PolygonMode polygonMode,
                                               float lineWidth, int patchControlPoints, int depthBias,
                                               float slopeScaledDepthBias) {
    QSharedPointer<RGResource> existingRes = mGraph->findResource(name);
    if (existingRes) {
        if (auto pipeRes = qSharedPointerCast<RGPipeline>(existingRes)) {
            qWarning("RGBuilder::setupGraphicsPipeline: Resource '%s' already exists. Returning existing.",
                     qPrintable(name));
            return RGPipelineRef(pipeRes);
        }
        qCritical(
            "RGBuilder::setupGraphicsPipeline: Resource name '%s' already exists but is NOT a Pipeline (Type: %d)!",
            qPrintable(name), static_cast<int>(existingRes->type()));
        return RGPipelineRef();
    }
    // --- 验证依赖 ---
    if (!srbLayoutRef.isValid()) {
        qWarning("RGBuilder::setupGraphicsPipeline '%s': Invalid ShaderResourceBindings reference provided.",
                 qPrintable(name));
        return RGPipelineRef();
    }
    if (!renderTargetRef.isValid()) {
        qCritical("RGBuilder::setupGraphicsPipeline '%s': Invalid RenderTarget reference provided.", qPrintable(name));
        return RGPipelineRef();
    }
    if (!renderTargetRef.mResource) {
        qCritical(
            "RGBuilder::setupGraphicsPipeline '%s': RenderTarget reference is valid but points to null resource?!.",
            qPrintable(name));
        return RGPipelineRef();
    }
    for (const auto &stage: shaderStages) {
        if (!stage.shader().isValid()) {
            qCritical("RGBuilder::setupGraphicsPipeline '%s': Invalid shader stage provided.", qPrintable(name));
            return RGPipelineRef();
        }
    }

    int sampleCount = 1;
    if (renderTargetRef.isValid() && renderTargetRef.mResource) {
        if (auto rtDesc = qSharedPointerCast<RGRenderTarget>(renderTargetRef.mResource)) {
            if (rtDesc->depthStencilAttachmentRef().isValid()) {
                if (auto dsRbRef = qSharedPointerCast<RGRenderBuffer>(rtDesc->depthStencilAttachmentRef().mResource)) {
                    sampleCount = dsRbRef->sampleCount();
                } else if (auto dsTexRef = qSharedPointerCast<
                    RGTexture>(rtDesc->depthStencilAttachmentRef().mResource)) {
                    sampleCount = dsTexRef->sampleCount();
                }
            }
            if (sampleCount <= 0) sampleCount = 1;
        }
    }
    qDebug("RGBuilder::setupGraphicsPipeline '%s': Using sample count %d from RenderTarget '%s'.", qPrintable(name),
           sampleCount, qPrintable(renderTargetRef.mResource->name()));

    auto pipelineResource = QSharedPointer<RGPipeline>::create(
        name, srbLayoutRef, renderTargetRef, shaderStages, vertexInputLayout, sampleCount, topology, cullMode,
        frontFace, depthTest, depthWrite, depthOp, stencilTest, stencilFront, stencilBack, stencilReadMask,
        stencilWriteMask, targetBlends, polygonMode, lineWidth, patchControlPoints, depthBias, slopeScaledDepthBias);

    mGraph->registerResource(pipelineResource);
    return RGPipelineRef(pipelineResource);
}

QRhi *RGBuilder::rhi() const {
    return mGraph->getRhi();
}

QSharedPointer<ResourceManager> RGBuilder::resourceManager() const {
    return mGraph->getResourceManager();
}

QSharedPointer<World> RGBuilder::world() const {
    return mGraph->getWorld();
}

const QSize &RGBuilder::outputSize() const {
    return mGraph->getOutputSize();
}

QRhiRenderPassDescriptor *RGBuilder::getSwapchainRpDesc() const {
    return mGraph->getSwapChainRpDesc();
}
