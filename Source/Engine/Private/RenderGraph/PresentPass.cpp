#include "RenderGraph/PresentPass.h"

#include "rhi/qrhi.h"
#include "RenderGraph/RenderGraph.h"
#include "RenderGraph/RGBuilder.h"
#include "Resources/ShaderBundle.h"

PresentPass::PresentPass(const QString &name): RGPass(name) {
}

PresentPass::~PresentPass() {
}

void PresentPass::setup(RGBuilder &builder) {
    qInfo() << "PresentPass::setup -" << name();
    mRhi = builder.rhi();
    if (!mRhi) {
        qCritical("PresentPass::setup - RHI instance is null!");
        return;
    }
    // --- 声明输入依赖 ---
    mInput.sourceTexture = builder.readTexture("BaseColor");
    if (!mInput.sourceTexture.isValid()) {
        qCritical(
            "PresentPass::setup - Failed to declare read dependency on texture 'BaseColor'. Did BasePass run setup?");
        return;
    }
    qInfo() << "  Declared Read: RGTexture 'BaseColor'";

    // --- 设置 Sampler ---
    mBlitSamplerRef = builder.setupSampler("PresentSampler",
                                           QRhiSampler::Nearest, QRhiSampler::Nearest, QRhiSampler::None,
                                           QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge);
    if (!mBlitSamplerRef.isValid()) {
        qCritical("PresentPass::setup - Failed to setup PresentSampler.");
        return;
    }
    qInfo() << "  Setup Sampler: 'PresentSampler'";

    // --- 设置 SRB 布局 ---
    QVector<QRhiShaderResourceBinding> blitBindingsDesc = {
        QRhiShaderResourceBinding::sampledTexture(0, QRhiShaderResourceBinding::FragmentStage, nullptr, nullptr)
    };
    mBlitBindingsLayoutRef = builder.setupShaderResourceBindings("PresentPassBindings", blitBindingsDesc);
    if (!mBlitBindingsLayoutRef.isValid()) {
        qCritical("PresentPass::setup - Failed to setup PresentPassBindings.");
        return;
    }
    qInfo() << "  Setup SRB Layout: 'PresentPassSRBLayout'";
    // 加载Shaders
    ShaderBundle::getInstance()->loadShader("Shaders/fullscreen", {
                                                {":/shaders/fullscreen.vert.qsb", QRhiShaderStage::Vertex},
                                                {
                                                    ":/shaders/fullscreen.frag.qsb", QRhiShaderStage::Fragment
                                                }
                                            });
    QRhiShaderStage vs = ShaderBundle::getInstance()->getShaderStage("Shaders/fullscreen", QRhiShaderStage::Vertex);
    QRhiShaderStage fs = ShaderBundle::getInstance()->getShaderStage("Shaders/fullscreen", QRhiShaderStage::Fragment);
    if (!vs.shader().isValid() || !fs.shader().isValid()) {
        qFatal("PresentPass::setup - Failed to load fullscreen shaders.");
        return;
    }
    qInfo() << "  Loaded Shaders: 'Shaders/fullscreen'";

    // --- 设置图形管线 ---
    RGRenderTargetRef swapChainProxyRT = builder.getRenderTarget("SwapChainRenderTargetProxy");
    if (!swapChainProxyRT.isValid()) {
        qCritical("PresentPass::setup - Failed to get valid SwapChainRenderTargetProxy resource or its descriptor.");
        return;
    }

    qInfo() << "  Obtained Ref to SwapChainRenderTargetProxy for pipeline setup.";

    mBlitPipelineRef = builder.setupGraphicsPipeline("PresentPipeline",
                                                     mBlitBindingsLayoutRef,
                                                     swapChainProxyRT,
                                                     {vs, fs},
                                                     {},
                                                     QRhiGraphicsPipeline::Triangles,
                                                     QRhiGraphicsPipeline::None,
                                                     QRhiGraphicsPipeline::CCW,
                                                     false, false,
                                                     QRhiGraphicsPipeline::Always
    );
    if (!mBlitPipelineRef.isValid()) {
        qCritical("PresentPass::setup - Failed to setup graphics pipeline 'PresentPipeline'.");
        return;
    }
    qInfo() << "  Setup Graphics Pipeline: 'PresentPipeline'";

    qInfo() << "PresentPass::setup finished successfully.";
}

void PresentPass::execute(QRhiCommandBuffer *cmdBuffer) {
    if (!mInput.sourceTexture.isValid()) {
        qWarning("PresentPass: No valid source texture");
        return;
    }

    /// --- 获取所需 RHI 资源 ---
    QRhiGraphicsPipeline *blitPipeline = mBlitPipelineRef.get();
    QRhiSampler *blitSampler = mBlitSamplerRef.get();
    QRhiTexture *sourceTexture = mInput.sourceTexture.get();

    QRhiSwapChain *swapChain = mGraph->getCurrentSwapChain();
    if (!blitPipeline || !blitSampler || !sourceTexture || !swapChain || !mRhi) {
        qWarning(
            "PresentPass::execute [%s] - Missing prerequisites (pipeline, sampler, source texture, swapchain, or RHI). Skipping.",
            qPrintable(name()));
        qWarning() << "  Pipeline:" << blitPipeline << "(Ref:" << mBlitPipelineRef.isValid() << ")";
        qWarning() << "  Sampler:" << blitSampler << "(Ref:" << mBlitSamplerRef.isValid() << ")";
        qWarning() << "  SrcTex:" << sourceTexture << "(Ref:" << mInput.sourceTexture.isValid() << ")";
        qWarning() << "  SwapChain:" << swapChain;
        qWarning() << "  RHI:" << mRhi;
        return;
    }

    QRhiRenderTarget *currentTarget = swapChain->currentFrameRenderTarget();
    if (!currentTarget) {
        qWarning("PresentPass::execute [%s] - Failed to get current frame render target from swapchain. Skipping.",
                 qPrintable(name()));
        return;
    }

    // --- Begin Render Pass on SwapChain Target ---
    const QColor clearColor = QColor::fromRgbF(0.3f, 0.2f, 0.2f, 1.0f);
    cmdBuffer->beginPass(currentTarget, clearColor, {1.0f, 0}, nullptr);

    // --- Set Graphics 状态 ---
    cmdBuffer->setGraphicsPipeline(blitPipeline);
    const QSize outputPixelSize = currentTarget->pixelSize();
    cmdBuffer->setViewport(QRhiViewport(0, 0, outputPixelSize.width(), outputPixelSize.height()));
    cmdBuffer->setScissor({0, 0, outputPixelSize.width(), outputPixelSize.height()});

    // --- 创建设置 Shader 资源 ---
    QScopedPointer<QRhiShaderResourceBindings> srb(mRhi->newShaderResourceBindings());
    if (!srb) {
        qWarning("PresentPass::execute [%s] - Failed to create new QRhiShaderResourceBindings.", qPrintable(name()));
        cmdBuffer->endPass();
        return;
    }
    srb->setBindings({
        QRhiShaderResourceBinding::sampledTexture(0, QRhiShaderResourceBinding::FragmentStage,
                                                  sourceTexture, blitSampler)
    });
    if (!srb->create()) {
        qWarning("PresentPass::execute [%s] - Failed to create SRB for blit.", qPrintable(name()));
        cmdBuffer->endPass();
        return;
    }
    cmdBuffer->setShaderResources(srb.data());

    cmdBuffer->draw(4);

    cmdBuffer->endPass();
}
