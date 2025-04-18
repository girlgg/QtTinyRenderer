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
#include "System/InputSystem.h"
#include "Scene/SystemManager.h"
#include "Scene/World.h"
#include "System/CameraSystem.h"

ViewWindow::ViewWindow(RhiHelper::InitParams inInitParmas)
    : RHIWindow(inInitParmas) {
    mSigInit.request();

    mResourceManager.reset(new ResourceManager());
    mWorld.reset(new World());
    mSystemManager.reset(new SystemManager());
    mSystemManager->addSystem<CameraSystem>();
}

ViewWindow::~ViewWindow() {
}

void ViewWindow::onInit() {
    mResourceManager->initialize(mRhi);

    mViewRenderSystem.reset(new RasterizeRenderSystem);

    initializeScene();
}

void ViewWindow::initializeScene() {
    // 创建相机实体
    EntityID cameraEntity = mWorld->createEntity();
    mWorld->addComponent<TransformComponent>(cameraEntity, {});
    float aspectRatio = mSwapChain->currentPixelSize().width() / (float) mSwapChain->currentPixelSize().height();
    TransformComponent *camTransform = mWorld->getComponent<TransformComponent>(cameraEntity);
    camTransform->setPosition(QVector3D(0, 0, 5));
    mWorld->addComponent<CameraComponent>(cameraEntity, {{}, aspectRatio, 90.0f, 0.1f, 1000.0f});
    mWorld->addComponent<CameraControllerComponent>(cameraEntity, {});

    const int numObjects = 20;
    for (int i = 0; i < numObjects; ++i) {
        EntityID entity = mWorld->createEntity();
        mWorld->addComponent<TransformComponent>(entity, {});
        mWorld->addComponent<MeshComponent>(entity, {});
        mWorld->addComponent<MaterialComponent>(entity, {});
        mWorld->addComponent<RenderableComponent>(entity, {});

        TransformComponent *transform = mWorld->getComponent<TransformComponent>(entity);
        transform->setPosition(QVector3D(QRandomGenerator::global()->generateDouble() * 10.0 - 10.0,
                                         QRandomGenerator::global()->generateDouble() * 10.0 - 5.0,
                                         QRandomGenerator::global()->generateDouble() * -10.0 - 5.0));
        transform->setRotation(QQuaternion::fromEulerAngles(
            QRandomGenerator::global()->generateDouble() * 360.0,
            QRandomGenerator::global()->generateDouble() * 360.0,
            QRandomGenerator::global()->generateDouble() * 360.0
        ));
        transform->setScale(QVector3D(0.5f, 0.5f, 0.5f) + QVector3D(QRandomGenerator::global()->generateDouble() * 0.5,
                                                                    QRandomGenerator::global()->generateDouble() * 0.5,
                                                                    QRandomGenerator::global()->generateDouble() *
                                                                    0.5));

        MeshComponent *mesh = mWorld->getComponent<MeshComponent>(entity);
        if (QRandomGenerator::global()->bounded(2) == 0) {
            mesh->meshResourceId = BUILTIN_CUBE_MESH_ID;
        } else {
            mesh->meshResourceId = BUILTIN_PYRAMID_MESH_ID;
        }
        // 注意：这里我们只设置了 meshResourceId。实际的顶点/索引数据由 ResourceManager 加载。
        // 如果需要加载自定义的 Mesh，需要先调用 ResourceManager::loadMeshFromData

        // 设置材质 (可以保持不变或添加更多随机性)
        MaterialComponent *mat = mWorld->getComponent<MaterialComponent>(entity);
        mat->albedoMapResourceId = ":/img/Images/container2.png";
    }

    // 创建灯光实体
    // 1. 创建一个方向光
    EntityID dirLightEntity = mWorld->createEntity();
    mWorld->addComponent<TransformComponent>(dirLightEntity, {});
    // 设置方向光的方向 (通过旋转 Transform 实现)
    TransformComponent *dirLightTransform = mWorld->getComponent<TransformComponent>(dirLightEntity);
    dirLightTransform->setRotation(QQuaternion::fromEulerAngles(45.0f, -45.0f, 0.0f)); // 指向某个方向
    mWorld->addComponent<LightComponent>(dirLightEntity, {
                                             {},
                                             LightType::Directional, {1.0f, 1.0f, 1.0f}, 0.6f,
                                             {}, 1, 0, 0, {},
                                             {0.1f, 0.1f, 0.1f}, {0.6f, 0.6f, 0.6f}, {0.5f, 0.5f, 0.5f}
                                         });

    // 2. 创建几个点光源
    int numPointLightsToCreate = qMin(MAX_POINT_LIGHTS, 3);
    for (int i = 0; i < numPointLightsToCreate; ++i) {
        EntityID pointLightEntity = mWorld->createEntity();
        mWorld->addComponent<TransformComponent>(pointLightEntity, {});
        TransformComponent *pointLightTransform = mWorld->getComponent<TransformComponent>(pointLightEntity);
        pointLightTransform->setPosition(QVector3D(QRandomGenerator::global()->generateDouble() * 15.0 - 7.5,
                                                   QRandomGenerator::global()->generateDouble() * 5.0,
                                                   QRandomGenerator::global()->generateDouble() * -15.0 - 2.5));
        QVector3D randomColor(QRandomGenerator::global()->generateDouble(),
                              QRandomGenerator::global()->generateDouble(),
                              QRandomGenerator::global()->generateDouble());
        randomColor.normalize();

        mWorld->addComponent<LightComponent>(pointLightEntity, {
                                                 {},
                                                 LightType::Point, randomColor, 1.5f,
                                                 {},
                                                 1.0f, 0.07f, 0.017f,
                                                 {},
                                                 {}, {},
                                                 {}
                                             });
    }
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

    mSystemManager->updateAll(mWorld.get(), mCpuFrameTime);

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
    if (cameraEntity != INVALID_ENTITY) {
        if (CameraComponent *camera = mWorld->getComponent<CameraComponent>(cameraEntity)) {
            mWorld->getComponent<CameraComponent>(cameraEntity)->mAspect =
                    renderTarget->pixelSize().width() / (float) renderTarget->pixelSize().height();
        }
    }
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
