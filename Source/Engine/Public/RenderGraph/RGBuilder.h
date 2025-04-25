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

    RGRenderBufferRef setupRenderBuffer(const QString &name, QRhiRenderBuffer::Type type, const QSize &size,
                                        int sampleCount = 1, QRhiRenderBuffer::Flags flags = {});

    RGRenderTargetRef setupRenderTarget(const QString &name, const QVector<RGTextureRef> &colorRefs,
                                        RGResourceRef dsRef = {});

    RGRenderTargetRef getRenderTarget(const QString &name);

    // --- RHI对象设置 ---

    RGSamplerRef setupSampler(const QString &name,
                              QRhiSampler::Filter magFilter,
                              QRhiSampler::Filter minFilter,
                              QRhiSampler::Filter mipmapMode,
                              QRhiSampler::AddressMode addressU,
                              QRhiSampler::AddressMode addressV,
                              QRhiSampler::AddressMode addressW = QRhiSampler::Repeat);

    RGShaderResourceBindingsRef setupShaderResourceBindings(const QString &name,
                                                            const QVector<QRhiShaderResourceBinding> &bindings);

    // 一个 Pass 会写入这个纹理
    RGTextureRef writeTexture(const QString &name, const QSize &size, QRhiTexture::Format format,
                              int sampleCount = 1, QRhiTexture::Flags flags = {});

    // 一个 Pass 会读取这个纹理
    RGTextureRef readTexture(const QString &name);

    // 声入深度/模板缓冲
    RGRenderBufferRef writeDepthStencil(const QString &name, const QSize &size, int sampleCount = 1);

    // 读取深度缓冲
    RGRenderBufferRef readDepthStencil(const QString &name);

    RGPipelineRef setupGraphicsPipeline(const QString &name,
                                        RGShaderResourceBindingsRef srbLayoutRef,
                                        RGRenderTargetRef renderTargetRef,
                                        const QVector<QRhiShaderStage> &shaderStages,
                                        const QRhiVertexInputLayout &vertexInputLayout,
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
                                        float lineWidth = 1.0f, int patchControlPoints = 0, int depthBias = 0,
                                        float slopeScaledDepthBias = 0.0f);

    // --- Accessors ---

    QRhi *rhi() const;

    QSharedPointer<ResourceManager> resourceManager() const;

    QSharedPointer<World> world() const;

    const QSize &outputSize() const;

    QRhiRenderPassDescriptor *getSwapchainRpDesc() const;

protected:
    RenderGraph *mGraph;

    RGPass *mCurrentPass;
};
