#include <QCoreApplication>
#include <QGuiApplication>
#include <QPlatformSurfaceEvent>
#include <rhi/qrhi.h>

#include "UI/ViewWidgets/RHIWindow.h"
#include "Graphics/RasterizeRenderSystem.h"
#include "Scene/InputSystem.h"

RHIWindow::RHIWindow(RhiHelper::InitParams inInitParmas)
    : mInitParams(inInitParmas) {
    switch (mInitParams.backend) {
        case QRhi::OpenGLES2:
            setSurfaceType(OpenGLSurface);
            break;
        case QRhi::Vulkan:
            setSurfaceType(VulkanSurface);
            break;
        case QRhi::D3D11:
            setSurfaceType(Direct3DSurface);
            break;
        case QRhi::Metal:
            setSurfaceType(MetalSurface);
            break;
        default:
            break;
    }
}

RHIWindow::~RHIWindow() {
    mDepthStencilBuffer.reset();
    mSwapChainPassDesc.reset();
    mRhi.reset();
}

void RHIWindow::exposeEvent(QExposeEvent *expose_event) {
    if (isExposed()) {
        if (!mRunning) {
            mRunning = true;
            initializeInternal();
        }
        mNotExposed = false;
    }

    const QSize surfaceSize = mHasSwapChain ? mSwapChain->surfacePixelSize() : QSize();

    if ((!isExposed() || (mHasSwapChain && surfaceSize.isEmpty())) && mRunning) {
        mNotExposed = true;
    }

    if (isExposed() && !surfaceSize.isEmpty()) {
        renderInternal();
    }
}

bool RHIWindow::event(QEvent *event) {
    switch (event->type()) {
        case QEvent::UpdateRequest:
            renderInternal();
            break;
        case QEvent::PlatformSurface:
            if (static_cast<QPlatformSurfaceEvent *>(event)->surfaceEventType() ==
                QPlatformSurfaceEvent::SurfaceAboutToBeDestroyed) {
                mHasSwapChain = false;
                onExit();
                mSwapChain.reset();
            }
            break;
        default:
            break;
    }
    return QWindow::event(event);
}

void RHIWindow::initializeInternal() {
    mRhi = RhiHelper::create(mInitParams.backend, mInitParams.rhiFlags, this);
    if (!mRhi)
        qFatal("Failed to create RHI backend");
    mSwapChain.reset(mRhi->newSwapChain());
    mDepthStencilBuffer.reset(mRhi->newRenderBuffer(QRhiRenderBuffer::DepthStencil, QSize(), mInitParams.sampleCount,
                                                    QRhiRenderBuffer::UsedWithSwapChainOnly));

    // 绑定渲染目标窗口
    mSwapChain->setWindow(this);
    // 设置深度模板缓冲区
    mSwapChain->setDepthStencil(mDepthStencilBuffer.get());
    // 设置多重采样（MSAA）的采样数
    mSwapChain->setSampleCount(mInitParams.sampleCount);
    // 设置 swapchain 的标志，比如是否启用 VSync、是否支持 sRGB 等。
    mSwapChain->setFlags(mInitParams.swapChainFlags);
    // 创建一个与当前 swapchain 配置兼容的 render pass 描述对象。
    mSwapChainPassDesc.reset(mSwapChain->newCompatibleRenderPassDescriptor());
    // 将刚刚创建的 render pass descriptor 设置给 swapchain。
    mSwapChain->setRenderPassDescriptor(mSwapChainPassDesc.get());
    // 创建或者根据当前窗口尺寸调整 swapchain。
    mSwapChain->createOrResize();

    onInit();

    mHasSwapChain = true;

    if (mInitParams.enableStat) {
        mCpuFrameTimer.start();
    }
}

void RHIWindow::renderInternal() {
    if (!mHasSwapChain || mNotExposed) {
        return;
    }
    // 等待上一帧完成才能绘制下一帧，否则会报错
    mRhi->finish();

    if (mSwapChain->currentPixelSize() != mSwapChain->surfacePixelSize() || mNewlyExposed) {
        resizeInternal();
        if (!mHasSwapChain)
            return;
        mNewlyExposed = false;
    }

    if (mNeedResize && !(QGuiApplication::mouseButtons() & Qt::LeftButton)) {
        onResize(mSwapChain->currentPixelSize());
        mNeedResize = false;
    }

    QRhi::FrameOpResult r = mRhi->beginFrame(mSwapChain.get(), mInitParams.beginFrameFlags);
    if (r == QRhi::FrameOpSwapChainOutOfDate) {
        resizeInternal();
        if (!mHasSwapChain)
            return;
        if (mInitParams.enableStat) {
            CpuFrameCounter = 0;
            mCpuFrameTimer.restart();
        }
        r = mRhi->beginFrame(mSwapChain.get());
    }
    if (r != QRhi::FrameOpSuccess) {
        requestUpdate();
        return;
    }

    if (mInitParams.enableStat) {
        CpuFrameCounter += 1;
        mCpuFrameTime = mCpuFrameTimer.elapsed();
        TimeCounter += mCpuFrameTime;
        mCpuFrameTimer.restart();

        if (TimeCounter > 1000) {
            mFps = CpuFrameCounter;
            CpuFrameCounter = 0;
            TimeCounter = 0;
        }
    }

    onRenderTick();

    mRhi->endFrame(mSwapChain.get(), mInitParams.endFrameFlags);

    if (!mInitParams.swapChainFlags.testFlag(QRhiSwapChain::NoVSync))
        requestUpdate();
    else
        QCoreApplication::postEvent(this, new QEvent(QEvent::UpdateRequest));
}

void RHIWindow::resizeInternal() {
    mHasSwapChain = mSwapChain->createOrResize();
    mNeedResize = true;
}
