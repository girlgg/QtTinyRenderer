#pragma once
#include <QMatrix4x4>
#include <Qimage>

#include "Light.h"
#include "SceneManager.h"
#include "CommonType.h"
#include "Component/MeshComponent.h"
#include "Component/TransformComponent.h"

class QRhiSampler;
class QRhiTexture;
class QRhiShaderResourceBindings;
class QRhiBuffer;
class QRhiGraphicsPipeline;
class QRhiRenderPassDescriptor;
class QRhiResourceUpdateBatch;
class QRhiCommandBuffer;
class QRhi;

struct alignas(16) UniformBlock {
    QGenericMatrix<4, 4, float> model;
    QGenericMatrix<4, 4, float> view;
    QGenericMatrix<4, 4, float> projection;
};

struct alignas(16) LightingBlock {
    QVector3DAlign ambient{0.1f, 0.1f, 0.1f};
    QVector3DAlign diffuse{0.8f, 0.8f, 0.8f};
    QVector3DAlign specular{1.0f, 1.0f, 1.0f};
    QVector3DAlign direction{0.0f, -1.0f, 0.0f};

    // DirLight dirLight;
    // PointLight pointLights[4];
    // SpotLight spotLights[4];
};

struct CameraBlock {
    QVector3D viewPos;
};

class Primitive {
public:
    Primitive();

    void setRhi(QRhi *rhi);

    void setSampleCount(const int samples) { mSampleCount = samples; }
    int getSampleCount() const { return mSampleCount; }

    void setScale(QVector3D scale);

    void setScale(float scale);

    QVector3D getLocation();

    void setLocation(const QVector3D &translation);

    const QQuaternion &getRotation() const;

    void setRotation(const QQuaternion &rotation);

    QMatrix4x4 getModelMatrix() const;

    void setViewMatrix(const QMatrix4x4 &viewMatrix);

    void setProjectionMatrix(const QMatrix4x4 &projectionMatrix);

    void setProjectionWithoutSpaceCorr(const QMatrix4x4 &projectionMatrix);

    QRhiGraphicsPipeline *getPipeline() const { return mPipeline; }

    void initResources(QRhiRenderPassDescriptor *rp);

    void setColorAttCount(const int count) { mColorAttCount = count; }

    void setDepthWrite(bool enable) { mDepthWrite = enable; }

    void releaseResources();

    void resize(const QSize &pixelSize);

    void queueResourceUpdates(QRhiResourceUpdateBatch *resourceUpdates);

    void queueDraw(QRhiCommandBuffer *cb, const QSize &outputSizeInPixels);

private:
    TransformComponent mTransform;

    static const QVector<VertexData> sVertexData;
    static const QVector<quint16> sIndexData;

    QRhiBuffer *mVertexBuffer = nullptr;
    QRhiBuffer *mIndexBuffer = nullptr;
    QRhiBuffer *mUniformBuffer = nullptr;
    QRhiBuffer *mLightingBuffer = nullptr;
    QRhiBuffer *mCameraBuffer = nullptr;
    QRhiGraphicsPipeline *mPipeline = nullptr;
    QRhiShaderResourceBindings *mShaderResourceBindings = nullptr;

    QRhi *mRhi = nullptr;

    QImage mImage;
    QRhiTexture *mTexture = nullptr;
    QRhiSampler *mSampler = nullptr;

    QMatrix4x4 mProjectionMatrix;
    QMatrix4x4 mViewMatrix;

    int mSampleCount = 1;
    int mColorAttCount = 1;

    mutable bool mDirty = false;
    bool mDepthWrite = false;
    bool mVertexBufferReady = false;
    bool mIndexBufferReady = false;
    bool mTextureReady = false;
};

class Cube : public Primitive {
public:
    Cube(QRhi *rhi);
};
