#pragma once
#include <QWidget>

#include "ECSCore.h"
#include "UI/ViewWidgets/RHIWindow.h"

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

signals:
    void fpsUpdated(float deltaTime, int fps);

protected:
    void onInit() override;

    void initializeScene();

    void initRhiResource();

    void onRenderTick() override;

    void onResize(const QSize &inSize) override;


    void setCameraPerspective();

private:
    QRhiSignal mSigInit;

    QSharedPointer<ResourceManager> mResourceManager;
    QScopedPointer<RasterizeRenderSystem> mViewRenderSystem;
    QScopedPointer<SystemManager> mSystemManager;

    QSharedPointer<World> mWorld;

    QPoint mLastMousePos;

    EntityID cameraEntity;

    QSet<Qt::Key> mPressedKeys;
    bool mCaptureMouse = false;
};

class ViewRenderWidget final : public QWidget {
    Q_OBJECT

public:
    explicit ViewRenderWidget(QWidget *parent = nullptr);

    ~ViewRenderWidget() override;

signals:
    void fpsUpdated(float deltaTime, int fps);

public slots:
    void onFpsUpdated(float deltaTime, int fps);

public:
    ViewWindow *mViewRenderWindow;
    QWidget *mViewWindowContainer;
    QVBoxLayout *mViewLayout;
};
