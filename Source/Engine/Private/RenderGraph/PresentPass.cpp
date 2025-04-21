#include "RenderGraph/PresentPass.h"

#include "RenderGraph/RGBuilder.h"
#include "Resources/ShaderBundle.h"

PresentPass::PresentPass(const QString &name): RGPass(name) {
}

PresentPass::~PresentPass() {
}

void PresentPass::setup(RGBuilder &builder) {
    mRhi = builder.rhi();
    if (!mInput.sourceTexture.isValid()) {
        qWarning("PresentPass setup: Input source texture is not valid.");
        return;
    }

    // Import the swapchain render target's texture into the graph
    // This assumes the swapchain RT is available via RHIWindow or similar
    // We need a way to get the current QRhiTexture from the swapchain.
    // Let's assume the RenderGraph is created with access to the swapchain RT.
    // THIS PART IS TRICKY - how does the graph know the *current* swapchain image?
    // Option 1: Graph::execute() takes the current RT.
    // Option 2: RHIWindow updates an 'external' resource handle in the graph each frame.
    // Let's assume Option 2 for now: Graph holds a special handle updated externally.

    // For now, let's just assume we get the swapchain texture somehow via the builder
    // This needs refinement based on how swapchain is managed with the graph.
    // Placeholder: Assume builder can give us the imported swapchain texture handle.
    // mOutput.swapChain = builder.getSwapChainOutput(); // Imaginary builder function

    // Alternative: The graph execution logic handles the final blit outside of a regular pass.
    // Let's proceed with the pass approach for now, assuming builder imports it.
    // This needs a mechanism in RHIWindow/ViewWindow to provide the current target.
    // We'll simulate this by importing a placeholder. The actual target is set in execute().


    // Setup blit pipeline (simple fullscreen textured quad)
    // builder.setupSampler(mBlitSampler, "PresentSampler", QRhiSampler::Nearest, QRhiSampler::Nearest, QRhiSampler::None,
    // QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge);

    // Assuming ShaderManager provides fullscreen VS/FS
    // QShader fsQuad = ShaderBundle::getInstance()->getShader("fullscreen.frag"); // Need a simple texture fragment shader
    // QShader vsQuad = ShaderBundle::getInstance()->getShader("fullscreen.vert"); // Basic passthrough vertex shader
    // if (!fsQuad.isValid() || !vsQuad.isValid()) {
    // qWarning("PresentPass: Failed to load fullscreen shaders.");
    // Create basic shaders programmatically as fallback?
    return;
    // }


    // builder.setupShaderResourceBindings(mBlitBindings, "PresentBindings", {
    // QRhiShaderResourceBinding::sampledTexture(
    // 0, QRhiShaderResourceBinding::FragmentStage, mInput.sourceTexture.get(),
    // mBlitSampler.get())
    // });

    // Need the render pass descriptor for the *swapchain* render target.
    // This is another piece of information the graph needs access to.
    // Assume it's passed during graph construction or accessible via RHIWindow.
    // QRhiRenderPassDescriptor *swapChainRpDesc = builder.rhi()->getSwapChainRenderPassDescriptor();
    // Need access to this

    // if (!swapChainRpDesc) {
    // qWarning("PresentPass::setup: Could not get swapchain render pass descriptor.");
    // return;
    // }

    // QRhiGraphicsPipeline::State pipelineState;
    // pipelineState.shaderResourceBindings = mBlitBindings.get();
    // pipelineState.sampleCount = 1; // Swapchain is typically 1x MSAA
    // pipelineState.renderPassDesc = swapChainRpDesc; // Use swapchain's RPDesc
    // pipelineState.shaderStages = {
    //     QRhiShaderStage(QRhiShaderStage::Vertex, vsQuad),
    //     QRhiShaderStage(QRhiShaderStage::Fragment, fsQuad)
    // };
    // // Disable depth/stencil, blending for simple blit
    // pipelineState.depthTest = false;
    // pipelineState.depthWrite = false;
    // pipelineState.stencilTest = false;
    //
    // mBlitPipeline = builder.setupGraphicsPipeline("PresentPipeline", pipelineState, swapChainRpDesc);
}

void PresentPass::execute(QRhiCommandBuffer *cmdBuffer) {
    // The actual swapchain render target is determined *just before* execution.
    // The RenderGraph executor should provide the correct target.
    // QRhiRenderTarget *currentSwapChainTarget = mRhi->getFrameSwapChainRenderTarget(); // Assumes RHI provides this
    // if (!currentSwapChainTarget || !mBlitPipeline || !mBlitBindings || !mInput.sourceTexture.isValid()) {
    // qWarning("PresentPass::execute prerequisites not met or swapchain target unavailable.");
    // return;
    // }

    // Update SRB if the input texture changed (shouldn't if graph manages it)
    // Might need update if source texture handle points to a different underlying texture?
    // For safety, maybe recreate/update bindings here - check if needed.

    const QColor clearColor = QColor::fromRgbF(0.0f, 0.0f, 0.0f, 0.0f); // Don't clear if just blitting over
    const QRhiDepthStencilClearValue dsClearValue = {}; // Don't clear depth/stencil

    // cmdBuffer->beginPass(currentSwapChainTarget, clearColor, dsClearValue, nullptr, QRhiCommandBuffer::DontClearColor);
    // Don't clear
    //
    // cmdBuffer->setGraphicsPipeline(mBlitPipeline.get());
    // const QSize outputSize = currentSwapChainTarget->pixelSize();
    // cmdBuffer->setViewport({0, 0, (float) outputSize.width(), (float) outputSize.height()});
    // cmdBuffer->setScissor({0, 0, outputSize.width(), outputSize.height()});
    // cmdBuffer->setShaderResources(mBlitBindings.get());
    // cmdBuffer->draw(4); // Draw fullscreen quad
    //
    // cmdBuffer->endPass();
}

PresentPass *PresentPass::setSource(RGTextureRef source) {
    mInput.sourceTexture = source;
    return this;
}
