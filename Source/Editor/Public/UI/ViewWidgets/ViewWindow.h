#pragma once
#include <QWidget>

#include "ECSCore.h"
#include "UI/ViewWidgets/RHIWindow.h"

class RenderGraph;
class SystemManager;
class World;
class ResourceManager;
class RasterizeRenderSystem;
class RhiRenderSystem;
class Camera;
class QVBoxLayout;

class ViewWindow : public RHIWindow {
    Q_OBJECT

public:
    explicit ViewWindow(RhiHelper::InitParams inInitParmas);

    ~ViewWindow() override;

    QSharedPointer<World> getWorld() const { return mWorld; }
    QSharedPointer<ResourceManager> getResourceManager() const { return mResourceManager; }

signals:
    void sceneInitialized();

    void fpsUpdated(float deltaTime, int fps);

protected:
    void onInit() override;

    void onExit() override;

    void initializeScene();

    void initRhiResource();

    void onRenderTick() override;

    void onResize(const QSize &inSize) override;

    void setCameraPerspective();

    virtual void defineRenderGraph(RenderGraph *graph);

    void updateRenderGraphResources(const QSize &newSize);

private:
    QRhiSignal mSigInit;

    QSharedPointer<ResourceManager> mResourceManager;
    QSharedPointer<SystemManager> mSystemManager;

    QSharedPointer<World> mWorld;

    QSharedPointer<RenderGraph> mRenderGraph;

    QPoint mLastMousePos;

    EntityID mCameraEntity;

    QSet<Qt::Key> mPressedKeys;
    bool mCaptureMouse = false;
};

class ViewRenderWidget final : public QWidget {
    Q_OBJECT

public:
    explicit ViewRenderWidget(QWidget *parent = nullptr);

    ~ViewRenderWidget() override;

    QSharedPointer<World> getWorld() const;

    QSharedPointer<ResourceManager> getResourceManager() const;

signals:
    void fpsUpdated(float deltaTime, int fps);

    void sceneInitialized();

public slots:
    void onFpsUpdated(float deltaTime, int fps);

public:
    ViewWindow *mViewRenderWindow;
    QWidget *mViewWindowContainer;
    QVBoxLayout *mViewLayout;
};
