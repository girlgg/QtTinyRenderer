#pragma once

#include <QMainWindow>

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
class SceneManager;

class EditorMainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit EditorMainWindow(QWidget *parent = nullptr);

    ~EditorMainWindow() override;

private:
    void InitEdgeLayout();

    void InitContent();

    EditorToolBar* TopToolBar;

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
    EditorDockWidget* TransformDock{nullptr};
    TransformEditor *ObjectTransformEditor{nullptr};

    ViewRenderWidget *ViewCentralWidget{nullptr};
};
