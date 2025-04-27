#pragma once

#include <QMainWindow>

#include "ECSCore.h"

class LightEditor;
class TextureEditor;
enum class TextureType;
struct TransformUpdateData;
class ResourceManager;
class World;
class ModelImporter;
class SceneTreeWidget;
class QTreeWidget;
class EditorDockWidget;
class EditorToolBar;
class QToolButton;
class QMenu;
class QAction;
class EditorStatusBar;
class TransformEditor;
class ViewRenderWidget;

class EditorMainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit EditorMainWindow(QWidget *parent = nullptr);

    ~EditorMainWindow() override;

private slots:
    void onImportModel();

    void onModelImported();

    void onImportFailed(const QString &error);

    void updateTransformFromEditor(EntityID entityId, const TransformUpdateData &data);

    void onTextureChanged(EntityID entityId, TextureType type, const QString &newPath);

    void onSceneInitialized();

    void onCreateCube();

    void onCreateSphere();

    void onCreatePointLight();

    void onCreateDirectionalLight();

    void onSceneSelectionChanged(EntityID entityId);

private:
    void setupModelImporter();

    void InitEdgeLayout();

    void InitContent();

    EditorToolBar *TopToolBar;

    QToolButton *ToolObjectBtn{nullptr};
    QMenu *ToolObjectMenu{nullptr};
    QAction *ObjectImportAction{nullptr};
    QAction *CreateCubeAction{nullptr};
    QAction *CreateSphereAction{nullptr};

    QToolButton *ToolLightBtn{nullptr};
    QMenu *ToolLightMenu{nullptr};
    QAction *CreatePointLightAction{nullptr};
    QAction *CreateDirectionalLightAction{nullptr};

    EditorStatusBar *BottomStatusBar{nullptr};

    EditorDockWidget *SceneTreeDock{nullptr};
    SceneTreeWidget *SceneTree{nullptr};
    EditorDockWidget *TransformDock{nullptr};
    TransformEditor *ObjectTransformEditor{nullptr};
    EditorDockWidget *TextureDock = nullptr;
    TextureEditor *ObjectTextureEditor = nullptr;
    EditorDockWidget* mLightDock = nullptr;
    LightEditor* mLightEditor = nullptr;

    ViewRenderWidget *ViewCentralWidget = nullptr;

    QSharedPointer<ModelImporter> mModelImporter;
    QSharedPointer<World> mWorld;
    QSharedPointer<ResourceManager> mResourceManager;
};
