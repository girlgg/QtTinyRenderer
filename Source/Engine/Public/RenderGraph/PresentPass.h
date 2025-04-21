#pragma once
#include "RGPass.h"
#include "RGResourceRef.h"

class PresentPass : public RGPass {
public:
    PresentPass(const QString &name);

    ~PresentPass() override;

    struct Input {
        RGTextureRef sourceTexture;
    };

    struct Output {
        RGTextureRef swapChain;
    };

    // 声明依赖
    void setup(RGBuilder &builder) override;

    void execute(QRhiCommandBuffer *cmdBuffer) override;

    PresentPass *setSource(RGTextureRef source);

private:
    Input mInput;
    Output mOutput;
    QSharedPointer<QRhiGraphicsPipeline> mBlitPipeline;
    QSharedPointer<QRhiSampler> mBlitSampler;
    QSharedPointer<QRhiShaderResourceBindings> mBlitBindings;
};
