#pragma once

#include <QSharedPointer>
#include <rhi/qrhi.h>

#include "RGResourceRef.h"

class RGResource {
public:
    enum class Type { Texture, Buffer, RenderBuffer, RenderTarget, Pipeline, ShaderResourceBindings, Sampler };

    RGResource(QString name, Type type) : mName(std::move(name)), mType(type) {
    }

    virtual ~RGResource() = default;

    virtual bool build(QRhi *inRhi) = 0;

    const QString &name() const { return mName; }
    Type type() const { return mType; }

    QSharedPointer<QRhiTexture> mRhiTexture = nullptr;
    QSharedPointer<QRhiBuffer> mRhiBuffer = nullptr;
    QSharedPointer<QRhiSampler> mRhiSampler = nullptr;
    QSharedPointer<QRhiRenderBuffer> mRhiRenderBuffer = nullptr;
    QSharedPointer<QRhiRenderTarget> mRhiRenderTarget = nullptr;
    QSharedPointer<QRhiGraphicsPipeline> mRhiGraphicsPipeline = nullptr;
    QSharedPointer<QRhiShaderResourceBindings> mRhiShaderResourceBindings = nullptr;

    QString mName;
    Type mType;
};

class RGTexture : public RGResource {
public:
    RGTexture(const QString &name, const QSize &size, QRhiTexture::Format format, int sampleCount = 1,
              QRhiTexture::Flags flags = {})
        : RGResource(name, Type::Texture), mSize(size), mFormat(format), mSampleCount(sampleCount), mFlags(flags) {
    }

    bool build(QRhi *inRhi) override;

    QSize size() const { return mSize; }
    QRhiTexture::Format format() const { return mFormat; }
    int sampleCount() const { return mSampleCount; }
    QRhiTexture::Flags flags() const { return mFlags; }

    void setSize(const QSize &size) { mSize = size; }

private:
    QSize mSize;
    QRhiTexture::Format mFormat;
    int mSampleCount;
    QRhiTexture::Flags mFlags;
};

class RGBuffer : public RGResource {
public:
    RGBuffer(const QString &name, QRhiBuffer::Type type, QRhiBuffer::UsageFlags usage, quint32 size)
        : RGResource(name, Type::Buffer), mBufType(type), mUsage(usage), mSize(size) {
    }

    bool build(QRhi *inRhi) override;

    QRhiBuffer::Type bufType() const { return mBufType; }
    QRhiBuffer::UsageFlags usage() const { return mUsage; }
    quint32 size() const { return mSize; }

private:
    QRhiBuffer::Type mBufType;
    QRhiBuffer::UsageFlags mUsage;
    quint32 mSize;
};

class RGRenderBuffer : public RGResource {
public:
    RGRenderBuffer(const QString &name, QRhiRenderBuffer::Type type, const QSize &size, int sampleCount = 1,
                   QRhiRenderBuffer::Flags flags = {})
        : RGResource(name, Type::RenderBuffer),
          mRbType(type), mSize(size), mSampleCount(sampleCount), mFlags(flags) {
    }

    bool build(QRhi *inRhi) override;

    QRhiRenderBuffer::Type rbType() const { return mRbType; }
    QSize size() const { return mSize; }
    int sampleCount() const { return mSampleCount; }
    QRhiRenderBuffer::Flags flags() const { return mFlags; }

    void setSize(const QSize &size) { mSize = size; }

private:
    QRhiRenderBuffer::Type mRbType;
    QSize mSize;
    int mSampleCount;
    QRhiRenderBuffer::Flags mFlags;
};

class RGRenderTarget : public RGResource {
public:
    RGRenderTarget(const QString &name,
                   const QVector<RGTextureRef> &colorAttachmentRefs,
                   RGResourceRef depthStencilRef = {});

    RGRenderTarget(const QString &name, QRhiRenderPassDescriptor *externalRpDesc);

    bool build(QRhi *inRhi) override;

    const QVector<RGTextureRef> &colorAttachmentRefs() const { return mColorAttachmentRefs; }
    RGResourceRef depthStencilAttachmentRef() const { return mDepthStencilAttachmentRef; }

    QRhiRenderPassDescriptor *renderPassDescriptor() const;

    bool isExternal() const { return mIsExternalRpDesc; }

    QSize getPixelSize() const;

    int getSampleCount() const;

private:
    QVector<RGTextureRef> mColorAttachmentRefs;
    RGResourceRef mDepthStencilAttachmentRef;
    QRhiRenderPassDescriptor *mRpDesc = nullptr;
    bool mIsExternalRpDesc = false;

    QScopedPointer<QRhiRenderPassDescriptor> mRpDescOwned;

    friend class RenderGraph;
};

class RGPipeline : public RGResource {
public:
    RGPipeline(const QString &name,
               RGShaderResourceBindingsRef srbLayoutRef,
               RGRenderTargetRef renderTargetRef,
               // --- Shader阶段 ---
               const QVector<QRhiShaderStage> &shaderStages,
               // --- 顶点输入 ---
               const QRhiVertexInputLayout &vertexInputLayout,
               // --- 管线状态 ---
               int sampleCount = 1,
               QRhiGraphicsPipeline::Topology topology = QRhiGraphicsPipeline::Triangles,
               QRhiGraphicsPipeline::CullMode cullMode = QRhiGraphicsPipeline::Back,
               QRhiGraphicsPipeline::FrontFace frontFace = QRhiGraphicsPipeline::CCW,

               bool depthTest = true,
               bool depthWrite = true,
               QRhiGraphicsPipeline::CompareOp depthOp = QRhiGraphicsPipeline::Less,

               bool stencilTest = false,
               QRhiGraphicsPipeline::StencilOpState stencilFront = {},
               QRhiGraphicsPipeline::StencilOpState stencilBack = {},
               quint32 stencilReadMask = 0xFF,
               quint32 stencilWriteMask = 0xFF,

               const QVector<QRhiGraphicsPipeline::TargetBlend> &targetBlends = {},

               QRhiGraphicsPipeline::PolygonMode polygonMode = QRhiGraphicsPipeline::Fill,
               float lineWidth = 1.0f,
               int patchControlPoints = 0,
               int depthBias = 0,
               float slopeScaledDepthBias = 0.0f);

    bool build(QRhi *inRhi) override;

    RGShaderResourceBindingsRef srbLayoutRef() const { return mSrbLayoutRef; }
    RGRenderTargetRef renderTargetRef() const { return mRenderTargetRef; }

    QRhiShaderResourceBindings *srbLayout() const { return mSrbLayoutRef.get(); }

    QRhiRenderPassDescriptor *rpDesc() const;

    const QVector<QRhiShaderStage> &shaderStages() const { return mShaderStages; }
    const QRhiVertexInputLayout &vertexInputLayout() const { return mVertexInputLayout; }
    int sampleCount() const { return mSampleCount; }
    QRhiGraphicsPipeline::Topology topology() const { return mTopology; }
    QRhiGraphicsPipeline::CullMode cullMode() const { return mCullMode; }
    QRhiGraphicsPipeline::FrontFace frontFace() const { return mFrontFace; }
    bool depthTestEnabled() const { return mDepthTest; }
    bool depthWriteEnabled() const { return mDepthWrite; }
    QRhiGraphicsPipeline::CompareOp depthOp() const { return mDepthOp; }
    bool stencilTestEnabled() const { return mStencilTest; }
    QRhiGraphicsPipeline::StencilOpState stencilFront() const { return mStencilFront; }
    QRhiGraphicsPipeline::StencilOpState stencilBack() const { return mStencilBack; }
    quint32 stencilReadMask() const { return mStencilReadMask; }
    quint32 stencilWriteMask() const { return mStencilWriteMask; }
    const QVector<QRhiGraphicsPipeline::TargetBlend> &targetBlends() const { return mTargetBlends; }
    QRhiGraphicsPipeline::PolygonMode polygonMode() const { return mPolygonMode; }
    float lineWidth() const { return mLineWidth; }
    int patchControlPoints() const { return mPatchControlPoints; }
    int depthBias() const { return mDepthBias; }
    float slopeScaledDepthBias() const { return mSlopeScaledDepthBias; }

private:
    RGShaderResourceBindingsRef mSrbLayoutRef;
    RGRenderTargetRef mRenderTargetRef;

    QVector<QRhiShaderStage> mShaderStages;
    QRhiVertexInputLayout mVertexInputLayout;
    int mSampleCount;
    QRhiGraphicsPipeline::Topology mTopology;
    QRhiGraphicsPipeline::CullMode mCullMode;
    QRhiGraphicsPipeline::FrontFace mFrontFace;
    bool mDepthTest;
    bool mDepthWrite;
    QRhiGraphicsPipeline::CompareOp mDepthOp;
    bool mStencilTest;
    QRhiGraphicsPipeline::StencilOpState mStencilFront;
    QRhiGraphicsPipeline::StencilOpState mStencilBack;
    quint32 mStencilReadMask;
    quint32 mStencilWriteMask;
    QVector<QRhiGraphicsPipeline::TargetBlend> mTargetBlends;
    QRhiGraphicsPipeline::PolygonMode mPolygonMode;
    float mLineWidth;
    int mPatchControlPoints;
    int mDepthBias;
    float mSlopeScaledDepthBias;
};

class RGShaderResourceBindings : public RGResource {
public:
    RGShaderResourceBindings(const QString &name, const QVector<QRhiShaderResourceBinding> &bindings)
        : RGResource(name, Type::ShaderResourceBindings), mBindings(bindings) {
    }

    bool build(QRhi *inRhi) override;

    const QVector<QRhiShaderResourceBinding> &bindings() const { return mBindings; }

private:
    QVector<QRhiShaderResourceBinding> mBindings;
};

class RGSampler : public RGResource {
public:
    RGSampler(const QString &name,
              QRhiSampler::Filter magFilter, QRhiSampler::Filter minFilter, QRhiSampler::Filter mipmapMode,
              QRhiSampler::AddressMode addressU, QRhiSampler::AddressMode addressV,
              QRhiSampler::AddressMode addressW = QRhiSampler::Repeat
    )
        : RGResource(name, Type::Sampler),
          mMagFilter(magFilter), mMinFilter(minFilter), mMipmapMode(mipmapMode),
          mAddressU(addressU), mAddressV(addressV), mAddressW(addressW) {
    }

    bool build(QRhi *inRhi) override;

    QRhiSampler::Filter magFilter() const { return mMagFilter; }
    QRhiSampler::Filter minFilter() const { return mMinFilter; }
    QRhiSampler::Filter mipmapMode() const { return mMipmapMode; }
    QRhiSampler::AddressMode addressU() const { return mAddressU; }
    QRhiSampler::AddressMode addressV() const { return mAddressV; }
    QRhiSampler::AddressMode addressW() const { return mAddressW; }

private:
    QRhiSampler::Filter mMagFilter;
    QRhiSampler::Filter mMinFilter;
    QRhiSampler::Filter mMipmapMode;
    QRhiSampler::AddressMode mAddressU;
    QRhiSampler::AddressMode mAddressV;
    QRhiSampler::AddressMode mAddressW;
};
