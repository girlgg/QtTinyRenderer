#pragma once
#include <QString>
#include <rhi/qrhi.h>

#include "RGResourceRef.h"

class World;
class ResourceManager;
class QSize;
class RGPass;
class RenderGraph;

class RGBuilder {
public:
    RGBuilder(RenderGraph *rg, RGPass *currentPass);

    // --- 资源创建和导入 ---

    RGTextureRef createTexture(const QString &name, const QSize &size, QRhiTexture::Format format,
                               int sampleCount = 1, QRhiTexture::Flags flags = {});

    RGBufferRef createBuffer(const QString &name, QRhiBuffer::Type type, QRhiBuffer::UsageFlags usage, quint32 size);

    RGTextureRef importTexture(const QString &name, QRhiTexture *existingTexture);

    RGBufferRef importBuffer(const QString &name, QRhiBuffer *existingBuffer);

    RGTextureRef importRenderTarget(const QString &name, QRhiRenderTarget *existingRenderTarget); // Helper

    // --- RHI对象设置 ---

    QSharedPointer<QRhiSampler> setupSampler(const QString &name, /* Sampler params */...);

    QSharedPointer<QRhiRenderTarget> setupRenderTarget(const QString &name,
                                                       const QRhiTextureRenderTargetDescription &desc);

    QSharedPointer<QRhiRenderBuffer> setupRenderBuffer(const QString &name, QRhiRenderBuffer::Type type,
                                                       const QSize &size, int sampleCount = 1,
                                                       QRhiRenderBuffer::Flags flags = {});

    QSharedPointer<QRhiGraphicsPipeline> setupGraphicsPipeline(const QString &name,
                                                               QSharedPointer<QRhiGraphicsPipeline> pipeline,
                                                               QRhiRenderPassDescriptor *rpDesc);

    QSharedPointer<QRhiShaderResourceBindings> setupShaderResourceBindings(
        const QString &name, const QVector<QRhiShaderResourceBinding> &bindings);

    // --- Accessors ---

    QRhi *rhi() const;

    QSharedPointer<ResourceManager> resourceManager() const;

    QSharedPointer<World> world() const;

    const QSize &outputSize() const;

private:
    RenderGraph *mGraph;
    RGPass *mCurrentPass;

    template<typename T>
    T *getRhiObject(RGResourceRef ref);
};

template<typename T>
T *RGBuilder::getRhiObject(RGResourceRef ref) {
}
