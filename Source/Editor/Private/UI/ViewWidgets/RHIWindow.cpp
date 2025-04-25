#include <QCoreApplication>
#include <QGuiApplication>
#include <QPlatformSurfaceEvent>
#include <rhi/qrhi.h>

#include "UI/ViewWidgets/RHIWindow.h"
#include "Graphics/RasterizeRenderSystem.h"
#include "System/InputSystem.h"

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
    mSwapChain.reset();
    mRhi.reset();
}

void RHIWindow::exposeEvent(QExposeEvent *expose_event) {
    if (isExposed()) {
        if (!mRunning) {
            mRunning = true;
            initializeInternal();
        }
        if (mNotExposed) {
            // If it was previously not exposed
            mNewlyExposed = true;
        }
        mNotExposed = false;
    } else {
        mNotExposed = true;
    }

    const QSize surfaceSize = mHasSwapChain ? mSwapChain->surfacePixelSize() : QSize();

    if (isExposed() && !mNotExposed && !surfaceSize.isEmpty()) {
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
                qInfo("PlatformSurface: SurfaceAboutToBeDestroyed");
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
    qInfo("Initializing RHIWindow...");
    mRhi = RhiHelper::create(mInitParams.backend, mInitParams.rhiFlags, this);
    if (!mRhi)
        qFatal("Failed to create RHI backend");

    mSwapChain.reset(mRhi->newSwapChain());
    QSize initialSize = size().isValid() ? size() * devicePixelRatio() : QSize(800, 600);
    mDepthStencilBuffer.reset(mRhi->newRenderBuffer(QRhiRenderBuffer::DepthStencil, initialSize,
                                                    mInitParams.sampleCount,
                                                    QRhiRenderBuffer::UsedWithSwapChainOnly));
    if (!mDepthStencilBuffer || !mDepthStencilBuffer->create()) {
        qFatal("Failed to create depth stencil buffer");
    }
    mDepthStencilBuffer->setName(QByteArrayLiteral("MainDepthStencilBuffer"));
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
    if (!mSwapChainPassDesc) {
        qFatal("Failed to create swap chain render pass descriptor");
    }
    mSwapChainPassDesc->setName(QByteArrayLiteral("SwapChainRenderPassDesc"));
    // 将刚刚创建的 render pass descriptor 设置给 swapchain。
    mSwapChain->setRenderPassDescriptor(mSwapChainPassDesc.get());

    // 创建或者根据当前窗口尺寸调整 swapchain。
    mHasSwapChain = mSwapChain->createOrResize();
    if (!mHasSwapChain) {
        qWarning("Initial SwapChain creation failed!");
    } else {
        qInfo() << "Initial SwapChain created/resized to" << mSwapChain->currentPixelSize();
    }

    onInit();

    if (mInitParams.enableStat) {
        mCpuFrameTimer.start();
    }
    qInfo("RHIWindow Initialization finished.");
}

void RHIWindow::renderInternal() {
    if (!mHasSwapChain || mNotExposed) {
        if (mNotExposed) requestUpdate();
        return;
    }
    // 等待上一帧绘制完成
    mRhi->finish();

    if (mSwapChain->currentPixelSize() != mSwapChain->surfacePixelSize() || mNewlyExposed || mNeedResize) {
        qInfo() << "Resize detected/needed. Current:" << mSwapChain->currentPixelSize() << "Surface:" << mSwapChain->
                surfacePixelSize() << "NewlyExposed:" << mNewlyExposed << "NeedResize:" << mNeedResize;
        resizeInternal();
        if (!mHasSwapChain) {
            qWarning("Resize failed, skipping frame.");
            return;
        }
        onResize(mSwapChain->currentPixelSize());
        mNewlyExposed = false;
        mNeedResize = false;
    }
    const QSize currentOutputSize = mSwapChain->currentPixelSize();
    if (currentOutputSize.isEmpty()) {
        qWarning("Current output size is empty, requesting update.");
        requestUpdate();
        return;
    }
    // --- Begin Frame ---
    QRhi::FrameOpResult r = mRhi->beginFrame(mSwapChain.get(), mInitParams.beginFrameFlags);
    if (r == QRhi::FrameOpSwapChainOutOfDate) {
        qInfo("BeginFrame: SwapChain out of date, forcing resize.");
        resizeInternal();
        if (!mHasSwapChain) return;
        onResize(mSwapChain->currentPixelSize());
        r = mRhi->beginFrame(mSwapChain.get());
    }
    if (r != QRhi::FrameOpSuccess) {
        qWarning("BeginFrame failed with status %d, requesting update.", static_cast<int>(r));
        requestUpdate();
        return;
    }
    // --- Frame Context ---
    QRhiCommandBuffer *cmdBuffer = mSwapChain->currentFrameCommandBuffer();
    // --- 更新统计 ---
    if (mInitParams.enableStat) {
        CpuFrameCounter += 1;
        float elapsed = mCpuFrameTimer.elapsed();
        mCpuFrameTime = elapsed;
        TimeCounter += elapsed;
        mCpuFrameTimer.restart();

        if (TimeCounter > 1000) {
            mFps = CpuFrameCounter;
            CpuFrameCounter = 0;
            TimeCounter = 0;
        }
    }

    onRenderTick();

    mRhi->endFrame(mSwapChain.get(), mInitParams.endFrameFlags);

    QCoreApplication::postEvent(this, new QEvent(QEvent::UpdateRequest));
}

void RHIWindow::resizeInternal() {
    mHasSwapChain = mSwapChain->createOrResize();
    mNeedResize = true;
}
