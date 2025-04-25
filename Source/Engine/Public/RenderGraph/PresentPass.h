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

    void setup(RGBuilder &builder) override;

    void execute(QRhiCommandBuffer *cmdBuffer) override;

private:
    Input mInput;
    RGPipelineRef mBlitPipelineRef;
    RGSamplerRef mBlitSamplerRef;
    RGShaderResourceBindingsRef mBlitBindingsLayoutRef;
};
