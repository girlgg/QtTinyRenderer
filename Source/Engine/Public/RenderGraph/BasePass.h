#pragma once
#include "ECSCore.h"
#include "RGPass.h"
#include "RGResourceRef.h"

struct InstanceUniformBlock;

class BasePass : public RGPass {
public:
    BasePass(const QString &name);

    struct Input {
        // base pass 无输入，从场景数据读取
    };

    struct Output {
        RGTextureRef baseColor;
        RGTextureRef depthStencil;
    };

    // 声明资源和管道
    void setup(RGBuilder &builder) override;

    // 记录绘制目录
    void execute(QRhiCommandBuffer *cmdBuffer) override;

private:
    void createPipelines(RGBuilder &builder, QRhiRenderPassDescriptor *rpDesc);

    void updateUniforms(QRhiResourceUpdateBatch *batch);

    void uploadInstanceData(QRhiResourceUpdateBatch *batch, int instanceCount);

    void findActiveCamera();

    Output mOutput;
    RGBufferRef mCameraUboRef;
    RGBufferRef mLightingUboRef;
    RGBufferRef mInstanceUboRef;
    QSharedPointer<QRhiGraphicsPipeline> mPipeline;
    QSharedPointer<QRhiShaderResourceBindings> mBaseSrbLayout;
    QSharedPointer<QRhiSampler> mDefaultSampler;

    // data buffer (CPU)
    QVector<InstanceUniformBlock> mInstanceDataBuffer;
    int mMaxInstances = 1024;
    quint32 mInstanceBlockAlignedSize = 0;

    EntityID mActiveCamera = INVALID_ENTITY;
};
