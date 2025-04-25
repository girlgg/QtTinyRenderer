#pragma once
#include "ECSCore.h"
#include "RGPass.h"
#include "RGResourceRef.h"

struct InstanceUniformBlock;

class BasePass : public RGPass {
public:
    BasePass(const QString &name);
    ~BasePass() override = default;

    struct Input {
        // base pass 无输入，从场景数据读取
    };

    struct Output {
        RGTextureRef baseColor;
        RGRenderBufferRef depthStencil;
    };

    // 声明资源和管道
    void setup(RGBuilder &builder) override;

    // 记录绘制目录
    void execute(QRhiCommandBuffer *cmdBuffer) override;

    Output getOutput() const { return mOutput; }

private:
    void updateUniforms(QRhiResourceUpdateBatch *batch);

    void uploadInstanceData(QRhiResourceUpdateBatch *batch, int instanceCount);

    void findActiveCamera();

    Output mOutput;

    RGRenderTargetRef mRenderTargetRef;
    RGBufferRef mCameraUboRef;
    RGBufferRef mLightingUboRef;
    RGBufferRef mInstanceUboRef;
    RGPipelineRef mPipelineRef;
    RGShaderResourceBindingsRef mBaseSrbLayoutRef;
    RGSamplerRef mDefaultSamplerRef;

    // data buffer (CPU)
    QVector<InstanceUniformBlock> mInstanceDataBuffer;
    int mMaxInstances = 1024;
    quint32 mInstanceBlockAlignedSize = 0;

    EntityID mActiveCamera = INVALID_ENTITY;
};
