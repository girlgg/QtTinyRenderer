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
#include "RenderGraph/BasePass.h"
#include "RenderGraph/PresentPass.h"
#include "RenderGraph/RenderGraph.h"
#include "RenderGraph/RGBuilder.h"
#include "Resources/ResourceManager.h"
#include "Scene/Camera.h"
#include "System/InputSystem.h"
#include "Scene/SystemManager.h"
#include "Scene/World.h"
#include "System/CameraSystem.h"

ViewWindow::ViewWindow(RhiHelper::InitParams inInitParmas)
    : RHIWindow(inInitParmas) {
    mResourceManager = QSharedPointer<ResourceManager>::create();
    mWorld = QSharedPointer<World>::create();
    mSystemManager = QSharedPointer<SystemManager>::create();
}

ViewWindow::~ViewWindow() {
}

void ViewWindow::onInit() {
    qInfo("ViewWindow::onInit - Initializing scene and resources...");

    mResourceManager->initialize(mRhi);
    mSystemManager->addSystem<CameraSystem>();

    initializeScene();

    qInfo("ViewWindow::onInit - Creating RenderGraph...");
    if (!mSwapChainPassDesc) {
        qFatal("Swapchain RenderPassDescriptor is null during ViewWindow::onInit!");
        return;
    }
    mRenderGraph = QSharedPointer<RenderGraph>::create(
        mRhi.get(),
        mResourceManager,
        mWorld,
        mSwapChain->currentPixelSize(),
        mSwapChainPassDesc.get()
    );

    defineRenderGraph(mRenderGraph.get());

    qInfo("ViewWindow::onInit - Compiling initial RenderGraph...");
    mRenderGraph->compile();

    mSigInit.request();

    qInfo("ViewWindow::onInit finished.");
}

void ViewWindow::onExit() {
    qInfo("ViewWindow::onExit - Releasing RHI resources...");
    mRenderGraph.reset();
    mResourceManager->releaseRhiResources();
}

void ViewWindow::initializeScene() {
    qInfo("ViewWindow::initializeScene - Setting up initial entities...");
    // 创建相机实体
    EntityID cameraEntity = mWorld->createEntity();
    mWorld->addComponent<TransformComponent>(cameraEntity, {});
    // 初始化屏幕比例
    float aspectRatio = mSwapChain->currentPixelSize().width() / (float) mSwapChain->currentPixelSize().height();
    TransformComponent *camTransform = mWorld->getComponent<TransformComponent>(cameraEntity);
    camTransform->setPosition(QVector3D(0, 0, 5));
    camTransform->rotate(180, QVector3D(0, 1, 0));
    mWorld->addComponent<CameraComponent>(cameraEntity, {{}, aspectRatio, 90.0f, 0.1f, 1000.0f});
    mWorld->addComponent<CameraControllerComponent>(cameraEntity, {});
    mCameraEntity = cameraEntity;

    mResourceManager->loadMeshFromData(BUILTIN_CUBE_MESH_ID,
                                       DEFAULT_CUBE_VERTICES,
                                       DEFAULT_CUBE_INDICES);
    mResourceManager->loadMeshFromData(BUILTIN_PYRAMID_MESH_ID,
                                       DEFAULT_PYRAMID_VERTICES,
                                       DEFAULT_PYRAMID_INDICES);

    const int numObjects = 2;
    for (int i = 0; i < numObjects; ++i) {
        EntityID entity = mWorld->createEntity();
        mWorld->addComponent<TransformComponent>(entity, {});
        mWorld->addComponent<MeshComponent>(entity, {});
        mWorld->addComponent<MaterialComponent>(entity, {});
        mWorld->addComponent<RenderableComponent>(entity, {});

        TransformComponent *transform = mWorld->getComponent<TransformComponent>(entity);
        transform->setPosition(QVector3D(QRandomGenerator::global()->generateDouble() * 10.0 - 5.0,
                                         QRandomGenerator::global()->generateDouble() * 10.0 - 5.0,
                                         QRandomGenerator::global()->generateDouble() * -5.0 - 5.0));
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
    emit sceneInitialized();
}

void ViewWindow::initRhiResource() {
    setCameraPerspective();
}

void ViewWindow::onRenderTick() {
    if (mInitParams.enableStat) {
        emit fpsUpdated(mCpuFrameTime, mFps);
    }
    if (mSigInit.ensure()) {
        initRhiResource();
    }
    // --- 更新ECS系统 ---
    if (mSystemManager) {
        mSystemManager->updateAll(mWorld.get(), mCpuFrameTime);
    }

    // --- 设置和执行渲染图 ---
    if (!mRhi || !mSwapChain || !mResourceManager || !mWorld) return;

    const QSize currentOutputSize = mSwapChain->currentPixelSize();
    if (currentOutputSize.isEmpty()) return;

    QRhiCommandBuffer *cmdBuffer = mSwapChain->currentFrameCommandBuffer();
    if (!cmdBuffer) {
        qWarning("ViewWindow::onRenderTick - Failed to get command buffer.");
        return;
    }

    if (!mRenderGraph->isCompiled()) {
        qWarning("ViewWindow::onRenderTick - RenderGraph is not compiled. Attempting recompile.");
        if (!mRenderGraph->getOutputSize().isValid() || mRenderGraph->getOutputSize().width() <= 0 || mRenderGraph->
            getOutputSize().height() <= 0) {
            qWarning("  RenderGraph output size is invalid, trying to set from swapchain.");
            if (!currentOutputSize.isEmpty()) {
                mRenderGraph->setOutputSize(currentOutputSize);
            } else {
                qCritical("  Cannot recompile RenderGraph: Both RG output size and swapchain size are invalid!");
                return;
            }
        }
        mRenderGraph->compile();
        if (!mRenderGraph->isCompiled()) {
            qCritical("ViewWindow::onRenderTick - RenderGraph failed to recompile. Skipping execution.");
            return;
        }
    }

    // 为当前帧创建渲染图
    mRenderGraph->setCommandBuffer(cmdBuffer);
    mRenderGraph->execute(mSwapChain.get());
}

void ViewWindow::onResize(const QSize &inSize) {
    if (!mRhi || !mRenderGraph || inSize.isEmpty()) {
        qWarning("ViewWindow::onResize - RHI or RenderGraph not ready, or size is empty. Skipping resize logic.");
        return;
    }
    updateRenderGraphResources(inSize);

    setCameraPerspective();
}

void ViewWindow::setCameraPerspective() {
    QRhiRenderTarget *renderTarget = mSwapChain->currentFrameRenderTarget();
    if (mCameraEntity != INVALID_ENTITY) {
        if (CameraComponent *camera = mWorld->getComponent<CameraComponent>(mCameraEntity)) {
            mWorld->getComponent<CameraComponent>(mCameraEntity)->mAspect =
                    renderTarget->pixelSize().width() / (float) renderTarget->pixelSize().height();
        }
    }
}

void ViewWindow::defineRenderGraph(RenderGraph *graph) {
    BasePass *basePass = graph->addPass<BasePass>("BasePass");
    PresentPass *presentPass = graph->addPass<PresentPass>("PresentPass");
}

void ViewWindow::updateRenderGraphResources(const QSize &newSize) {
    if (!mRenderGraph) return;

    qInfo() << "ViewWindow::updateRenderGraphResources - Updating RG for size" << newSize;
    mRenderGraph->setOutputSize(newSize); // Inform RG about the new size

    qInfo() << "ViewWindow::updateRenderGraphResources - Recompiling RenderGraph...";
    mRenderGraph->compile();
    qInfo() << "ViewWindow::updateRenderGraphResources - RenderGraph recompiled.";
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
    connect(mViewRenderWindow, &ViewWindow::sceneInitialized, this, &ViewRenderWidget::sceneInitialized);
}

ViewRenderWidget::~ViewRenderWidget() {
    delete mViewRenderWindow;
}

QSharedPointer<World> ViewRenderWidget::getWorld() const {
    return mViewRenderWindow ? mViewRenderWindow->getWorld() : nullptr;
}

QSharedPointer<ResourceManager> ViewRenderWidget::getResourceManager() const {
    return mViewRenderWindow ? mViewRenderWindow->getResourceManager() : nullptr;
}

void ViewRenderWidget::onFpsUpdated(float deltaTime, int fps) {
    emit fpsUpdated(deltaTime, fps);
}
