#pragma once

#include <QWindow>

#include "RhiHelper.h"

class RenderSystem;
class QRhiSwapChain;

class RHIWindow : public QWindow {
    Q_OBJECT

public:
    explicit RHIWindow(RhiHelper::InitParams inInitParmas);

    ~RHIWindow() override;

protected:
    virtual void onInit() {
    }

    virtual void onRenderTick() {
    }

    virtual void onResize(const QSize &inSize) {
    }

    virtual void onExit() {
    }

    void exposeEvent(QExposeEvent *) override;

    bool event(QEvent *event) override;

private:
    void initializeInternal();

    void renderInternal();

    void resizeInternal();

protected:
    RhiHelper::InitParams mInitParams;

    QSharedPointer<QRhi> mRhi;
    QSharedPointer<QRhiSwapChain> mSwapChain;
    QSharedPointer<QRhiRenderBuffer> mDepthStencilBuffer;
    QSharedPointer<QRhiRenderPassDescriptor> mSwapChainPassDesc;

    RenderSystem* mViewRenderSystem;

    QElapsedTimer mCpuFrameTimer;

    int mFps = 0;
    int CpuFrameCounter = 0;
    float TimeCounter = 0;
    float mCpuFrameTime;

    bool mRunning = false;
    bool mHasSwapChain = false;
    bool mNotExposed = false;
    bool mNewlyExposed = false;
    bool mNeedResize = false;
};
