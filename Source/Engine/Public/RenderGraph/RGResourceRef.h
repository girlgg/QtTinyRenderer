#pragma once
#include <rhi/qrhi.h>

class RGSampler;
class RGShaderResourceBindings;
class RGPipeline;
class RGRenderTarget;
class RGBuffer;
class RGRenderBuffer;
class RGTexture;
class RGResource;

class RGResourceRef {
public:
    RGResourceRef(const QSharedPointer<RGResource> &res = nullptr);

    bool isValid() const;

    QSharedPointer<RGResource> mResource = nullptr;

    bool operator==(const RGResourceRef &other) const { return mResource == other.mResource; }
    bool operator!=(const RGResourceRef &other) const { return mResource != other.mResource; }
};

class RGTextureRef : public RGResourceRef {
public:
    RGTextureRef(const QSharedPointer<RGTexture> &tex = nullptr);

    QRhiTexture *get() const;

    QSize pixelSize() const;

    QRhiTexture::Format format() const;

    int sampleCount() const;

    QRhiTexture::Flags flags() const;
};

class RGBufferRef : public RGResourceRef {
public:
    RGBufferRef(const QSharedPointer<RGBuffer> &buf = nullptr);

    QRhiBuffer *get() const;

    quint32 size() const;

    QRhiBuffer::Type bufType() const;

    QRhiBuffer::UsageFlags usage() const;
};

class RGRenderBufferRef : public RGResourceRef {
public:
    RGRenderBufferRef(const QSharedPointer<RGRenderBuffer> &rb = nullptr);

    QRhiRenderBuffer *get() const;

    QSize pixelSize() const;

    int sampleCount() const;

    QRhiRenderBuffer::Type rbType() const;

    QRhiRenderBuffer::Flags flags() const;
};

class RGRenderTargetRef : public RGResourceRef {
public:
    RGRenderTargetRef(const QSharedPointer<RGRenderTarget> &rt = nullptr);

    QRhiRenderTarget *get() const;

    QRhiRenderPassDescriptor *renderPassDescriptor() const;

    QSize pixelSize() const;

    int sampleCount() const;

    const QVector<RGTextureRef> &colorAttachmentRefs() const;

    RGResourceRef depthStencilAttachmentRef() const;
};

class RGPipelineRef : public RGResourceRef {
public:
    RGPipelineRef(const QSharedPointer<RGPipeline> &pipe = nullptr);

    QRhiGraphicsPipeline *get() const;
};

class RGShaderResourceBindingsRef : public RGResourceRef {
public:
    RGShaderResourceBindingsRef(const QSharedPointer<RGShaderResourceBindings> &srb = nullptr);

    QRhiShaderResourceBindings *get() const;

    const QVector<QRhiShaderResourceBinding>& bindings() const;
};

class RGSamplerRef : public RGResourceRef {
public:
    RGSamplerRef(const QSharedPointer<RGSampler> &sampler = nullptr);

    QRhiSampler *get() const;
};
