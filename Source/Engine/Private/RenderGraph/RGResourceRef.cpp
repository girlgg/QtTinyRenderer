#include <utility>

#include "RenderGraph/RGResourceRef.h"

#include "RenderGraph/RGResource.h"

RGResourceRef::RGResourceRef(const QSharedPointer<RGResource> &res)
    : mResource(res) {
}

bool RGResourceRef::isValid() const {
    return !mResource.isNull();
}

RGTextureRef::RGTextureRef(const QSharedPointer<RGTexture> &tex)
    : RGResourceRef(tex) {
}

QRhiTexture *RGTextureRef::get() const {
    return mResource ? qSharedPointerCast<RGTexture>(mResource)->mRhiTexture.get() : nullptr;
}

QSize RGTextureRef::pixelSize() const {
    return mResource ? qSharedPointerCast<RGTexture>(mResource)->size() : QSize();
}

QRhiTexture::Format RGTextureRef::format() const {
    return mResource ? qSharedPointerCast<RGTexture>(mResource)->format() : QRhiTexture::UnknownFormat;
}

int RGTextureRef::sampleCount() const {
    return mResource ? qSharedPointerCast<RGTexture>(mResource)->sampleCount() : 0;
}

QRhiTexture::Flags RGTextureRef::flags() const {
    return mResource ? qSharedPointerCast<RGTexture>(mResource)->flags() : QRhiTexture::Flags();
}

RGBufferRef::RGBufferRef(const QSharedPointer<RGBuffer> &buf) : RGResourceRef(buf) {
}

QRhiBuffer *RGBufferRef::get() const {
    return mResource ? qSharedPointerCast<RGBuffer>(mResource)->mRhiBuffer.get() : nullptr;
}

quint32 RGBufferRef::size() const {
    return mResource ? qSharedPointerCast<RGBuffer>(mResource)->size() : 0;
}

QRhiBuffer::Type RGBufferRef::bufType() const {
    return mResource ? qSharedPointerCast<RGBuffer>(mResource)->bufType() : QRhiBuffer::Immutable;
}

QRhiBuffer::UsageFlags RGBufferRef::usage() const {
    {
        return mResource ? qSharedPointerCast<RGBuffer>(mResource)->usage() : QRhiBuffer::UsageFlags();
    }
}

RGRenderBufferRef::RGRenderBufferRef(const QSharedPointer<RGRenderBuffer> &rb) : RGResourceRef(rb) {
}

QRhiRenderBuffer *RGRenderBufferRef::get() const {
    return mResource ? qSharedPointerCast<RGRenderBuffer>(mResource)->mRhiRenderBuffer.get() : nullptr;
}

QSize RGRenderBufferRef::pixelSize() const {
    return mResource ? qSharedPointerCast<RGRenderBuffer>(mResource)->size() : QSize();
}

int RGRenderBufferRef::sampleCount() const {
    return mResource ? qSharedPointerCast<RGRenderBuffer>(mResource)->sampleCount() : 0;
}

QRhiRenderBuffer::Type RGRenderBufferRef::rbType() const {
    return mResource ? qSharedPointerCast<RGRenderBuffer>(mResource)->rbType() : QRhiRenderBuffer::Color;
}

QRhiRenderBuffer::Flags RGRenderBufferRef::flags() const {
    return mResource ? qSharedPointerCast<RGRenderBuffer>(mResource)->flags() : QRhiRenderBuffer::Flags();
}

RGRenderTargetRef::RGRenderTargetRef(const QSharedPointer<RGRenderTarget> &rt) : RGResourceRef(rt) {
}

QRhiRenderTarget *RGRenderTargetRef::get() const {
    if (!mResource) {
        qWarning("RGRenderTargetRef::get() called with null resource");
        return nullptr;
    }
    auto rt = qSharedPointerCast<RGRenderTarget>(mResource);
    if (!rt->mRhiRenderTarget) {
        qWarning("RGRenderTargetRef::get() called for %s which has null QRhiRenderTarget",
                 qPrintable(rt->name()));
        return nullptr;
    }
    return rt->mRhiRenderTarget.get();
}

QRhiRenderPassDescriptor *RGRenderTargetRef::renderPassDescriptor() const {
    QRhiRenderTarget *rt = get();
    return rt ? rt->renderPassDescriptor() : nullptr;
}

QSize RGRenderTargetRef::pixelSize() const {
    QRhiRenderTarget *rt = get();
    return rt ? rt->pixelSize() : QSize();
}

int RGRenderTargetRef::sampleCount() const {
    return mResource ? qSharedPointerCast<RGRenderTarget>(mResource)->getSampleCount() : 1;
}

const QVector<RGTextureRef> &RGRenderTargetRef::colorAttachmentRefs() const {
    static const QVector<RGTextureRef> emptyVec; // Return empty if null
    return mResource ? qSharedPointerCast<RGRenderTarget>(mResource)->colorAttachmentRefs() : emptyVec;
}

RGResourceRef RGRenderTargetRef::depthStencilAttachmentRef() const {
    return mResource ? qSharedPointerCast<RGRenderTarget>(mResource)->depthStencilAttachmentRef() : RGResourceRef();
}

RGPipelineRef::RGPipelineRef(const QSharedPointer<RGPipeline> &pipe) : RGResourceRef(pipe) {
}

QRhiGraphicsPipeline *RGPipelineRef::get() const {
    return mResource ? qSharedPointerCast<RGPipeline>(mResource)->mRhiGraphicsPipeline.get() : nullptr;
}

RGShaderResourceBindingsRef::RGShaderResourceBindingsRef(const QSharedPointer<RGShaderResourceBindings> &srb)
    : RGResourceRef(srb) {
}

QRhiShaderResourceBindings *RGShaderResourceBindingsRef::get() const {
    return mResource
               ? qSharedPointerCast<RGShaderResourceBindings>(mResource)->mRhiShaderResourceBindings.get()
               : nullptr;
}

const QVector<QRhiShaderResourceBinding> & RGShaderResourceBindingsRef::bindings() const {
    static const QVector<QRhiShaderResourceBinding> emptyVec;
    return mResource ? qSharedPointerCast<RGShaderResourceBindings>(mResource)->bindings() : emptyVec;
}

RGSamplerRef::RGSamplerRef(const QSharedPointer<RGSampler> &sampler) : RGResourceRef(sampler) {
}

QRhiSampler *RGSamplerRef::get() const {
    return mResource ? qSharedPointerCast<RGSampler>(mResource)->mRhiSampler.get() : nullptr;
}
