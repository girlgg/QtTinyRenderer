#include <QVBoxLayout>
#include <kernel/qevent.h>
#include <QRandomGenerator>

#include "UI/ViewWidgets/ViewWindow.h"

#include "Component/CameraComponent.h"
#include "Component/CameraControllerComponent.h"
#include "Component/LightComponent.h"
#include "Component/MaterialComponent.h"
#include "Component/MeshComponent.h"
#include "Component/TransformComponent.h"
#include "Component/RenderableComponent.h"
#include "Graphics/RasterizeRenderSystem.h"
#include "Resources/ResourceManager.h"
#include "Scene/Camera.h"
#include "Scene/InputSystem.h"
#include "Scene/SceneManager.h"
#include "Scene/World.h"

ViewWindow::ViewWindow(RhiHelper::InitParams inInitParmas)
    : RHIWindow(inInitParmas) {
    mSigInit.request();

    mResourceManager.reset(new ResourceManager());
    mWorld.reset(new World());
}

ViewWindow::~ViewWindow() {
}

void ViewWindow::onInit() {
    mResourceManager->initialize(mRhi);

    mViewRenderSystem.reset(new RasterizeRenderSystem);

    initializeScene();
}

void ViewWindow::initializeScene() {
    // Create Camera Entity
    EntityID cameraEntity = mWorld->createEntity();
    mWorld->addComponent<TransformComponent>(cameraEntity, {});
    float aspectRatio = mSwapChain->currentPixelSize().width() / (float) mSwapChain->currentPixelSize().height();
    mWorld->addComponent<CameraComponent>(cameraEntity, {{}, aspectRatio, 90.0f, 0.1f, 1000.0f});
    mWorld->addComponent<CameraControllerComponent>(cameraEntity, {}); // Assuming this exists
    // TransformComponent *camTransform = mWorld->getComponent<TransformComponent>(cameraEntity);
    // camTransform->setPosition(QVector3D(0.0f, 0.0f, -2.0f));

    // Create Cube Entity
    for (int i = 0; i < 30; ++i) {
        EntityID cubeEntity = mWorld->createEntity();
        mWorld->addComponent<TransformComponent>(cubeEntity, {});
        mWorld->addComponent<MeshComponent>(cubeEntity, {});
        mWorld->addComponent<MaterialComponent>(cubeEntity, {});
        mWorld->addComponent<RenderableComponent>(cubeEntity, {});

        TransformComponent *transform = mWorld->getComponent<TransformComponent>(cubeEntity);
        transform->setPosition(QVector3D(QRandomGenerator::global()->bounded(4) - 2,
                                         QRandomGenerator::global()->bounded(4) - 2,
                                         -5 - QRandomGenerator::global()->bounded(4)));
    }

    // Create Light Entity
    EntityID lightEntity = mWorld->createEntity();
    mWorld->addComponent<TransformComponent>(lightEntity, {}); // Pointing downwards-ish
    mWorld->addComponent<LightComponent>(lightEntity, {
                                             {}, LightType::Directional, {1, 1, 1}, 1.0f, {1, 1, 1}, 1, 0, 0, {}, {},
                                             {}, {0.1f, 0.1f, 0.1f}, {0.8f, 0.8f, 0.8f}
                                         });
}

void ViewWindow::initRhiResource() {
    mViewRenderSystem->initialize(mRhi.get(), mSwapChain.get(), mSwapChainPassDesc.get(), mWorld, mResourceManager);

    setCameraPerspective();
}

void ViewWindow::onRenderTick() {
    if (mInitParams.enableStat) {
        emit fpsUpdated(mCpuFrameTime, mFps);
    }
    if (mSigInit.ensure()) {
        initRhiResource();
    }

    SceneManager::get().tick(mCpuFrameTime);

    QRhiRenderTarget *renderTarget = mSwapChain->currentFrameRenderTarget();
    QRhiCommandBuffer *cmdBuffer = mSwapChain->currentFrameCommandBuffer();

    QRhiResourceUpdateBatch *batch = mRhi->nextResourceUpdateBatch();

    mViewRenderSystem->submitResourceUpdates(batch);

    cmdBuffer->resourceUpdate(batch);

    const QColor clearColor = QColor::fromRgbF(0.2f, 0.2f, 0.2f, 1.0f);
    const QRhiDepthStencilClearValue dsClearValue = {1.0f, 0};

    cmdBuffer->beginPass(renderTarget, clearColor, dsClearValue, nullptr);

    mViewRenderSystem->draw(cmdBuffer, mSwapChain->currentPixelSize());

    cmdBuffer->endPass();
}

void ViewWindow::onResize(const QSize &inSize) {
    mViewRenderSystem->resize(inSize);

    setCameraPerspective();
}

void ViewWindow::setCameraPerspective() {
    QRhiRenderTarget *renderTarget = mSwapChain->currentFrameRenderTarget();
    SceneManager::get().getCamera()->camera.mAspect =
            renderTarget->pixelSize().width() / (float) renderTarget->pixelSize().height();
}

ViewRenderWidget::ViewRenderWidget(QWidget *parent) {
    RhiHelper::InitParams initParams;
    initParams.backend = QRhi::Vulkan;
    initParams.enableStat = true;

    mViewRenderWindow = new ViewWindow(initParams);

    auto &inputSystem = InputSystem::get();
    mViewRenderWindow->installEventFilter(&inputSystem);

    mViewWindowContainer = createWindowContainer(mViewRenderWindow);
    mViewLayout = new QVBoxLayout(this);
    mViewLayout->addWidget(mViewWindowContainer);

    connect(mViewRenderWindow, &ViewWindow::fpsUpdated, this, &ViewRenderWidget::onFpsUpdated);
}

ViewRenderWidget::~ViewRenderWidget() {
    delete mViewRenderWindow;
}

void ViewRenderWidget::onFpsUpdated(float deltaTime, int fps) {
    emit fpsUpdated(deltaTime, fps);
}
