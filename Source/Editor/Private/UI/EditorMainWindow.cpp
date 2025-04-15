#include <QToolButton>
#include <QMenu>
#include <QSplitter>
#include <QTreeWidget>
#include <QVBoxLayout>

#include "UI/EditorMainWindow.h"
#include "UI/EdgesWidgets/EditorStatusBar.h"
#include "UI/EdgesWidgets/EditorToolBar.h"
#include "UI/EdgesWidgets/EditorDockWidget.h"
#include "UI/EdgesWidgets/TransformEditor.h"
#include "UI/ViewWidgets/ViewWindow.h"
#include "../../../Engine/Public/Scene/SceneManager.h"
#include "UI/EdgesWidgets/SceneTreeWidget.h"

EditorMainWindow::EditorMainWindow(QWidget *parent) : QMainWindow(parent) {
    resize(1080, 680);

    InitEdgeLayout();
    InitContent();

    if (ViewCentralWidget && BottomStatusBar) {
        connect(ViewCentralWidget, &ViewRenderWidget::fpsUpdated, BottomStatusBar, &EditorStatusBar::fpsUpdate);
    }
}

EditorMainWindow::~EditorMainWindow() {
}

void EditorMainWindow::InitEdgeLayout() {
    // 初始化状态栏(底部信息)
    BottomStatusBar = new EditorStatusBar(this);
    this->setStatusBar(BottomStatusBar);

    // // 创建工具栏
    auto *toolBar = new EditorToolBar("工具栏", this);
    toolBar->setAllowedAreas(Qt::TopToolBarArea | Qt::BottomToolBarArea);
    toolBar->setToolBarSpacing(3);
    toolBar->setToolButtonStyle(Qt::ToolButtonIconOnly);
    toolBar->setIconSize(QSize(25, 25));

    // 工具栏
    ToolObjectBtn = new QToolButton(this);
    ToolObjectBtn->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    ToolObjectBtn->setText("物体");
    ToolObjectBtn->setPopupMode(QToolButton::InstantPopup);
    ToolObjectMenu = new QMenu(ToolObjectBtn);
    ObjectImportAction = ToolObjectMenu->addAction("导入模型");
    CreateCubeAction = ToolObjectMenu->addAction("立方体");
    CreateSphereAction = ToolObjectMenu->addAction("球体");
    ToolObjectBtn->setMenu(ToolObjectMenu);
    toolBar->addWidget(ToolObjectBtn);

    ToolLightBtn = new QToolButton(this);
    ToolLightBtn->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    ToolLightBtn->setText("灯光");
    ToolLightBtn->setPopupMode(QToolButton::InstantPopup);
    ToolLightMenu = new QMenu(ToolLightBtn);
    CreatePointLightAction = ToolLightMenu->addAction("点光");
    CreateDirectionalLightAction = ToolLightMenu->addAction("定向光");
    ToolLightBtn->setMenu(ToolLightMenu);
    toolBar->addWidget(ToolLightBtn);

    this->addToolBar(Qt::TopToolBarArea, toolBar);

    // 右侧面板
    auto *rightSplitter = new QSplitter(Qt::Vertical, this);

    // 上方 - 场景物体树
    SceneTreeDock = new EditorDockWidget("场景物体树", this);
    SceneTree = new SceneTreeWidget(this);
    SceneTreeDock->setWidget(SceneTree);

    // 下方 - 变换编辑器
    TransformDock = new EditorDockWidget("变换编辑器", this);
    ObjectTransformEditor = new TransformEditor(this);
    TransformDock->setWidget(ObjectTransformEditor);

    connect(SceneTree, &SceneTreeWidget::objectSelected,
            ObjectTransformEditor, &TransformEditor::setCurrentObject);

    connect(ObjectTransformEditor, &TransformEditor::transformChanged,
            [](int id, QVector3D loc) { SceneManager::get().setObjectLocation(id, loc); });

    connect(&SceneManager::get(), &SceneManager::objectLocationUpdated,
            [this](int id, QVector3D loc) {
                if (ObjectTransformEditor->currentObjectId() == id) {
                    ObjectTransformEditor->updateFromSceneManager();
                }
            });

    SceneTree->refreshSceneTree();

    // 将两个dock添加到分割器中
    rightSplitter->addWidget(SceneTreeDock);
    rightSplitter->addWidget(TransformDock);

    // 设置分割比例
    rightSplitter->setStretchFactor(0, 2);
    rightSplitter->setStretchFactor(1, 1);

    // 创建右侧dock容器
    auto *rightDockContainer = new QWidget(this);
    auto *rightLayout = new QVBoxLayout(rightDockContainer);
    rightLayout->addWidget(rightSplitter);
    rightDockContainer->setLayout(rightLayout);

    // 添加到主窗口右侧
    auto *rightDock = new EditorDockWidget("右侧面板", this);
    rightDock->setWidget(rightDockContainer);
    this->addDockWidget(Qt::RightDockWidgetArea, rightDock);
    resizeDocks({rightDock}, {300}, Qt::Horizontal);
}

void EditorMainWindow::InitContent() {
    ViewCentralWidget = new ViewRenderWidget();

    setCentralWidget(ViewCentralWidget);
}
