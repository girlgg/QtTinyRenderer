#pragma once

#include <QSharedPointer>
#include <rhi/qrhi.h>

class QRhiShaderResourceBindings;
class QRhiGraphicsPipeline;
class QRhiRenderTarget;
class QRhiRenderBuffer;
class QRhiSampler;
class QRhiBuffer;
class QRhiTexture;

class RGResource {
public:
    enum class Type { Texture, Buffer };

    RGResource(QString name, Type type) : mName(std::move(name)), mType(type) {
    }

    virtual ~RGResource() = default;

    const QString &name() const { return mName; }
    Type type() const { return mType; }

    // TODO: Add reference counting for lifetime management within the graph
    // TODO: Add state tracking for automatic barriers (advanced)

    QString mName;
    Type mType;

    QSharedPointer<QRhiTexture> mRhiTexture = nullptr;
    QSharedPointer<QRhiBuffer> mRhiBuffer = nullptr;
    QSharedPointer<QRhiSampler> mRhiSampler = nullptr;
    QSharedPointer<QRhiRenderBuffer> mRhiRenderBuffer = nullptr;
    QSharedPointer<QRhiRenderTarget> mRhiRenderTarget = nullptr;
    QSharedPointer<QRhiGraphicsPipeline> mRhiGraphicsPipeline = nullptr;
    QSharedPointer<QRhiShaderResourceBindings> mRhiShaderResourceBindings = nullptr;
};

class RGTexture : public RGResource {
public:
    RGTexture(const QString &name, const QSize &size, QRhiTexture::Format format, int sampleCount = 1,
              QRhiTexture::Flags flags = {})
        : RGResource(name, Type::Texture), mSize(size), mFormat(format), mSampleCount(sampleCount), mFlags(flags) {
    }

    QSize size() const { return mSize; }
    QRhiTexture::Format format() const { return mFormat; }
    int sampleCount() const { return mSampleCount; }
    QRhiTexture::Flags flags() const { return mFlags; }

private:
    QSize mSize;
    QRhiTexture::Format mFormat;
    int mSampleCount;
    QRhiTexture::Flags mFlags;
    friend class RGBuilder;
    friend class RenderGraph;
};

class RGBuffer : public RGResource {
public:
    RGBuffer(const QString &name, QRhiBuffer::Type type, QRhiBuffer::UsageFlags usage, quint32 size)
        : RGResource(name, Type::Buffer), mBufType(type), mUsage(usage), mSize(size) {
    }

    QRhiBuffer::Type bufType() const { return mBufType; }
    QRhiBuffer::UsageFlags usage() const { return mUsage; }
    quint32 size() const { return mSize; }

private:
    QRhiBuffer::Type mBufType;
    QRhiBuffer::UsageFlags mUsage;
    quint32 mSize;
    friend class RGBuilder;
    friend class RenderGraph;
};
