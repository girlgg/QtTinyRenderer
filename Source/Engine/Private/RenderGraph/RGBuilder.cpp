#include "RenderGraph/RGBuilder.h"

RGBuilder::RGBuilder(RenderGraph *rg, RGPass *currentPass) {
}

RGTextureRef RGBuilder::createTexture(const QString &name, const QSize &size, QRhiTexture::Format format,
                                      int sampleCount, QRhiTexture::Flags flags) {
    return RGTextureRef();
}

RGBufferRef RGBuilder::createBuffer(const QString &name, QRhiBuffer::Type type, QRhiBuffer::UsageFlags usage,
                                    quint32 size) {
    return RGBufferRef();
}

RGTextureRef RGBuilder::importTexture(const QString &name, QRhiTexture *existingTexture) {
    return RGTextureRef();
}

RGBufferRef RGBuilder::importBuffer(const QString &name, QRhiBuffer *existingBuffer) {
    return RGBufferRef();
}

RGTextureRef RGBuilder::importRenderTarget(const QString &name, QRhiRenderTarget *existingRenderTarget) {
    return RGTextureRef();
}

QSharedPointer<QRhiSampler> RGBuilder::setupSampler(const QString &name, ...) {
    return nullptr;
}

QSharedPointer<QRhiRenderTarget> RGBuilder::setupRenderTarget(const QString &name,
                                                              const QRhiTextureRenderTargetDescription &desc) {
    return nullptr;
}

QSharedPointer<QRhiRenderBuffer> RGBuilder::setupRenderBuffer(const QString &name, QRhiRenderBuffer::Type type,
                                                              const QSize &size, int sampleCount,
                                                              QRhiRenderBuffer::Flags flags) {
    return nullptr;
}

QSharedPointer<QRhiGraphicsPipeline> RGBuilder::setupGraphicsPipeline(const QString &name,
                                                                      QSharedPointer<QRhiGraphicsPipeline> pipeline,
                                                                      QRhiRenderPassDescriptor *rpDesc) {
    return nullptr;
}

QSharedPointer<QRhiShaderResourceBindings> RGBuilder::setupShaderResourceBindings(const QString &name,
    const QVector<QRhiShaderResourceBinding> &bindings) {
}

QRhi *RGBuilder::rhi() const {
}

QSharedPointer<ResourceManager> RGBuilder::resourceManager() const {
    return nullptr;
}

QSharedPointer<World> RGBuilder::world() const {
    return nullptr;
}

const QSize &RGBuilder::outputSize() const {
    return QSize();
}
