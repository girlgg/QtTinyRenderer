#include "Scene/Primitive.h"

#include <rhi/qrhi.h>

#include "Component/MeshComponent.h"
#include "Component/TransformComponent.h"
#include "Resources/ShaderBundle.h"

const QVector<VertexData> Primitive::sVertexData;
const QVector<quint16> Primitive::sIndexData;
/*const QVector<VertexData> Primitive::sVertexData = {
    // 后
    {-0.5f, -0.5f, -0.5f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f},
    {0.5f, 0.5f, -0.5f, 0.0f, 0.0f, -1.0f, 1.0f, 1.0f},
    {0.5f, -0.5f, -0.5f, 0.0f, 0.0f, -1.0f, 1.0f, 0.0f},
    {-0.5f, 0.5f, -0.5f, 0.0f, 0.0f, -1.0f, 0.0f, 1.0f},
    {0.5f, 0.5f, 0.5f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f},
    {-0.5f, -0.5f, 0.5f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f},
    {0.5f, -0.5f, 0.5f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f},
    {-0.5f, 0.5f, 0.5f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f},
    {-0.5f, -0.5f, -0.5f, -1.0f, 0.0f, 0.0f, 0.0f, 1.0f},
    {-0.5f, 0.5f, 0.5f, -1.0f, 0.0f, 0.0f, 1.0f, 0.0f},
    {-0.5f, 0.5f, -0.5f, -1.0f, 0.0f, 0.0f, 1.0f, 1.0f},
    {-0.5f, -0.5f, 0.5f, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f},
    {0.5f, 0.5f, 0.5f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f},
    {0.5f, -0.5f, -0.5f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f},
    {0.5f, 0.5f, -0.5f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f},
    {0.5f, -0.5f, 0.5f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f},
    {0.5f, -0.5f, 0.5f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f},
    {-0.5f, -0.5f, -0.5f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f},
    {0.5f, -0.5f, -0.5f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f},
    {-0.5f, -0.5f, 0.5f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f},
    {-0.5f, 0.5f, -0.5f, 0.0f, -1.0f, 0.0f, 0.0f, 1.0f},
    {0.5f, 0.5f, 0.5f, 0.0f, -1.0f, 0.0f, 1.0f, 0.0f},
    {0.5f, 0.5f, -0.5f, 0.0f, -1.0f, 0.0f, 1.0f, 1.0f},
    {-0.5f, 0.5f, 0.5f, 0.0f, -1.0f, 0.0f, 0.0f, 0.0f}
};

const QVector<quint16> Primitive::sIndexData = {
    0, 1, 2, 3, 1, 0,
    4, 5, 6, 7, 5, 4,
    8, 9, 10, 11, 9, 8,
    12, 13, 14, 15, 13, 12,
    16, 17, 18, 19, 17, 16,
    20, 21, 22, 23, 21, 20
};*/

Primitive::Primitive() {
}

void Primitive::setRhi(QRhi *rhi) {
    if (mRhi != rhi) {
        releaseResources();
        mRhi = rhi;
    }
}

void Primitive::setScale(QVector3D scale) {
    mTransform.setScale(scale);
}

void Primitive::setScale(float scale) {
    setScale(QVector3D(scale, scale, scale));
}

QVector3D Primitive::getLocation() {
    return mTransform.position();
}

void Primitive::setLocation(const QVector3D &translation) {
    mTransform.setPosition(translation);
}

const QQuaternion &Primitive::getRotation() const {
    return mTransform.rotation();
}

void Primitive::setRotation(const QQuaternion &rotation) {
    mTransform.setRotation(rotation);
}

void Primitive::releaseResources() {
    if (mPipeline) {
        delete mPipeline;
        mPipeline = nullptr;
    }
    if (mShaderResourceBindings) {
        delete mShaderResourceBindings;
        mShaderResourceBindings = nullptr;
    }
    if (mUniformBuffer) {
        delete mUniformBuffer;
        mUniformBuffer = nullptr;
    }
    if (mVertexBuffer) {
        delete mVertexBuffer;
        mVertexBuffer = nullptr;
    }
    if (mIndexBuffer) {
        delete mIndexBuffer;
        mIndexBuffer = nullptr;
    }
    if (mTexture) {
        delete mTexture;
        mTexture = nullptr;
    }
    if (mSampler) {
        delete mSampler;
        mSampler = nullptr;
    }
    mVertexBufferReady = false;
    mIndexBufferReady = false;
}

void Primitive::resize(const QSize &pixelSize) {
}

void Primitive::queueResourceUpdates(QRhiResourceUpdateBatch *resourceUpdates) {
    if (mVertexBuffer && !mVertexBufferReady) {
        resourceUpdates->uploadStaticBuffer(mVertexBuffer, sVertexData.constData());
        mVertexBufferReady = true;
    }
    if (mIndexBuffer && !mIndexBufferReady) {
        resourceUpdates->uploadStaticBuffer(mIndexBuffer, sIndexData.constData());
        mIndexBufferReady = true;
    }

    if (mTexture && !mTextureReady) {
        resourceUpdates->uploadTexture(mTexture, mImage);
        mTextureReady = true;
    }

    UniformBlock ub;
    QMatrix4x4 modelMatrix = getModelMatrix();
    ub.model = modelMatrix.toGenericMatrix<4, 4>();
    ub.view = mViewMatrix.toGenericMatrix<4, 4>();
    ub.projection = mProjectionMatrix.toGenericMatrix<4, 4>();
    resourceUpdates->updateDynamicBuffer(mUniformBuffer, 0, sizeof(UniformBlock), &ub);

    LightingBlock lightingBlock;
    if (SceneManager::get().getDirectionalLight()) {
        DirLight *dirLight = SceneManager::get().getDirectionalLight();
        lightingBlock.ambient = dirLight->getAmbient();
        lightingBlock.diffuse = dirLight->getDiffuse();
        lightingBlock.specular = dirLight->getSpecular();
        lightingBlock.direction = dirLight->getDirection();
    }
    resourceUpdates->updateDynamicBuffer(mLightingBuffer, 0, sizeof(LightingBlock), &lightingBlock);

    CameraBlock cameraBlock;
    cameraBlock.viewPos = SceneManager::get().getCamera()->transform.position();
    resourceUpdates->updateDynamicBuffer(mCameraBuffer, 0, sizeof(CameraBlock), &cameraBlock);
}

void Primitive::queueDraw(QRhiCommandBuffer *cb, const QSize &outputSizeInPixels) {
    if (!mPipeline || !mVertexBufferReady || !mIndexBufferReady) {
        qFatal("Primitive not initialized");
        return;
    }

    cb->setGraphicsPipeline(mPipeline);
    cb->setViewport({0, 0, float(outputSizeInPixels.width()), float(outputSizeInPixels.height())});
    cb->setShaderResources(mShaderResourceBindings);

    QRhiCommandBuffer::VertexInput vbufBinding(mVertexBuffer, 0);
    cb->setVertexInput(0, 1, &vbufBinding, mIndexBuffer, 0, QRhiCommandBuffer::IndexUInt16);
    cb->drawIndexed(sIndexData.size());
}

QMatrix4x4 Primitive::getModelMatrix() const {
    return mTransform.worldMatrix();
}

void Primitive::setViewMatrix(const QMatrix4x4 &viewMatrix) {
    mViewMatrix = viewMatrix;
}

void Primitive::setProjectionMatrix(const QMatrix4x4 &projectionMatrix) {
    mProjectionMatrix = projectionMatrix;
}

void Primitive::setProjectionWithoutSpaceCorr(const QMatrix4x4 &projectionMatrix) {
    mProjectionMatrix = mRhi->clipSpaceCorrMatrix() * projectionMatrix;
}

void Primitive::initResources(QRhiRenderPassDescriptor *rp) {
    if (!mRhi) {
        qFatal("QRhi not initialized");
        return;
    }

    mVertexBuffer = mRhi->newBuffer(QRhiBuffer::Immutable, QRhiBuffer::VertexBuffer,
                                    sizeof(VertexData) * sVertexData.size());
    if (!mVertexBuffer->create()) {
        qFatal("Failed to create vertex buffer");
    }
    mIndexBuffer = mRhi->newBuffer(QRhiBuffer::Immutable, QRhiBuffer::IndexBuffer,
                                   sizeof(quint16) * sIndexData.size());
    if (!mIndexBuffer->create()) {
        qFatal("Failed to create index buffer");
    }
    mUniformBuffer = mRhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, sizeof(UniformBlock));
    if (!mUniformBuffer->create()) {
        qFatal("Failed to create uniform buffer");
    }
    mLightingBuffer = mRhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, sizeof(LightingBlock));
    if (!mLightingBuffer->create()) {
        qFatal("Failed to create uniform buffer");
    }
    mCameraBuffer = mRhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, sizeof(QVector3D));
    if (!mCameraBuffer->create()) {
        qFatal("Failed to create uniform buffer");
    }

    mImage = QImage(":/img/Images/container.png");
    mImage = mImage.convertToFormat(QImage::Format_RGBA8888);
    mTexture = mRhi->newTexture(QRhiTexture::RGBA8, mImage.size());
    if (!mTexture->create()) {
        qFatal("Failed to create texture");
    }

    mSampler = mRhi->newSampler(QRhiSampler::Linear,
                                QRhiSampler::Linear,
                                QRhiSampler::None,
                                QRhiSampler::ClampToEdge,
                                QRhiSampler::ClampToEdge);
    if (!mSampler->create()) {
        qFatal("Failed to create sampler");
    }

    QRhiVertexInputLayout inputLayout;
    inputLayout.setBindings({
        QRhiVertexInputBinding(sizeof(VertexData))
    });
    // inputLayout.setAttributes({
    //     QRhiVertexInputAttribute(0, 0, QRhiVertexInputAttribute::Float3, offsetof(VertexData, x)),
    //     QRhiVertexInputAttribute(0, 1, QRhiVertexInputAttribute::Float3, offsetof(VertexData, nx)),
    //     QRhiVertexInputAttribute(0, 2, QRhiVertexInputAttribute::Float2, offsetof(VertexData, u)),
    // });

    mPipeline = mRhi->newGraphicsPipeline();

    ShaderBundle::getInstance()->loadShader("Shaders/baseGeo", {
                                                {":/shaders/baseGeo.vert.qsb", QRhiShaderStage::Vertex},
                                                {":/shaders/baseGeo.frag.qsb", QRhiShaderStage::Fragment}
                                            });

    mPipeline->setShaderStages({
        ShaderBundle::getInstance()->getShaderStage("Shaders/baseGeo", QRhiShaderStage::Vertex),
        ShaderBundle::getInstance()->getShaderStage("Shaders/baseGeo", QRhiShaderStage::Fragment)
    });

    mPipeline->setVertexInputLayout(inputLayout);
    mPipeline->setSampleCount(mSampleCount);
    mPipeline->setRenderPassDescriptor(rp);
    mPipeline->setCullMode(QRhiGraphicsPipeline::Back); // 背面剔除
    mPipeline->setFrontFace(QRhiGraphicsPipeline::CCW); // 顶点序
    if (mDepthWrite) {
        // 深度测试
        mPipeline->setDepthTest(true);
        mPipeline->setDepthOp(QRhiGraphicsPipeline::Less);
        mPipeline->setDepthWrite(true);
    }

    // 创建着色器资源绑定
    mShaderResourceBindings = mRhi->newShaderResourceBindings();
    mShaderResourceBindings->setBindings({
        QRhiShaderResourceBinding::uniformBuffer(0, QRhiShaderResourceBinding::VertexStage, mUniformBuffer),
        QRhiShaderResourceBinding::sampledTexture(1, QRhiShaderResourceBinding::FragmentStage, mTexture, mSampler),
        QRhiShaderResourceBinding::uniformBuffer(2, QRhiShaderResourceBinding::FragmentStage, mLightingBuffer),
        QRhiShaderResourceBinding::uniformBuffer(3, QRhiShaderResourceBinding::FragmentStage, mCameraBuffer)
    });
    if (!mShaderResourceBindings->create()) {
        qFatal("Failed to create shader resource bindings");
    }
    mPipeline->setShaderResourceBindings(mShaderResourceBindings);

    if (!mPipeline->create()) {
        qFatal("Failed to create graphics pipeline");
    }
}
