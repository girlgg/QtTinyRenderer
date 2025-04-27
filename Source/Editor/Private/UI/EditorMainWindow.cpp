#include <QToolButton>
#include <QMenu>
#include <QSplitter>
#include <QVBoxLayout>
#include <QFileDialog>
#include <QMessageBox>
#include "UI/EditorMainWindow.h"

#include "CommonType.h"
#include "Component/LightComponent.h"
#include "Component/MaterialComponent.h"
#include "Component/MeshComponent.h"
#include "Component/RenderableComponent.h"
#include "Component/TransformComponent.h"
#include "Resources/ResourceManager.h"
#include "Scene/ModelImporter.h"
#include "Scene/World.h"
#include "UI/EdgesWidgets/EditorStatusBar.h"
#include "UI/EdgesWidgets/EditorToolBar.h"
#include "UI/EdgesWidgets/EditorDockWidget.h"
#include "UI/EdgesWidgets/LightEditor.h"
#include "UI/EdgesWidgets/TransformEditor.h"
#include "UI/ViewWidgets/ViewWindow.h"
#include "UI/EdgesWidgets/SceneTreeWidget.h"
#include "UI/EdgesWidgets/TextureEditor.h"

EditorMainWindow::EditorMainWindow(QWidget *parent) : QMainWindow(parent) {
    resize(1280, 720);

    InitContent();
    if (ViewCentralWidget) {
        mWorld = ViewCentralWidget->getWorld();
        mResourceManager = ViewCentralWidget->getResourceManager();
        if (!mWorld || !mResourceManager) {
            qCritical("Failed to get World or ResourceManager from ViewRenderWidget!");
        }
        connect(ViewCentralWidget, &ViewRenderWidget::sceneInitialized, this, &EditorMainWindow::onSceneInitialized);
    } else {
        qCritical("ViewCentralWidget is null!");
    }

    InitEdgeLayout();
    if (ViewCentralWidget && BottomStatusBar) {
        connect(ViewCentralWidget, &ViewRenderWidget::fpsUpdated, BottomStatusBar, &EditorStatusBar::fpsUpdate);
    }
    if (SceneTree) {
        connect(SceneTree, &SceneTreeWidget::objectSelected, this, &EditorMainWindow::onSceneSelectionChanged);
    }

    setupModelImporter();

    connect(ObjectImportAction, &QAction::triggered, this, &EditorMainWindow::onImportModel);
    connect(CreateCubeAction, &QAction::triggered, this, &EditorMainWindow::onCreateCube);
    connect(CreateSphereAction, &QAction::triggered, this, &EditorMainWindow::onCreateSphere);
    connect(CreatePointLightAction, &QAction::triggered, this, &EditorMainWindow::onCreatePointLight);
    connect(CreateDirectionalLightAction, &QAction::triggered, this, &EditorMainWindow::onCreateDirectionalLight);

    if (ObjectTransformEditor) {
        connect(ObjectTransformEditor, &TransformEditor::transformChanged,
                this, &EditorMainWindow::updateTransformFromEditor);
    }
    if (ObjectTextureEditor) {
        connect(ObjectTextureEditor, &TextureEditor::textureChanged,
                this, &EditorMainWindow::onTextureChanged);
    }
    if (ObjectTransformEditor && mWorld) {
        ObjectTransformEditor->setWorld(mWorld);
    } else if (!ObjectTransformEditor) {
        qWarning("ObjectTransformEditor is null during setup.");
    } else {
        qWarning("mWorld is null when setting TransformEditor world.");
    }

    if (ObjectTextureEditor && mWorld && mResourceManager) {
        ObjectTextureEditor->setWorld(mWorld);
        ObjectTextureEditor->setResourceManager(mResourceManager);
    } else if (!ObjectTextureEditor) {
        qWarning("ObjectTextureEditor is null during setup.");
    } else {
        qWarning("mWorld or mResourceManager is null when setting TextureEditor world/rm.");
    }

    if (mLightEditor && mWorld) {
        mLightEditor->setWorld(mWorld);
    } else if (!mLightEditor) {
        qWarning("mLightEditor is null during setup.");
    } else {
        qWarning("mWorld is null when setting LightEditor world.");
    }

    if (SceneTree && mWorld) {
        SceneTree->setWorld(mWorld);
    } else if (!SceneTree) {
        qWarning("SceneTree is null during setup.");
    } else {
        qWarning("mWorld is null when setting SceneTree world.");
    }

    qInfo("EditorMainWindow Initialization Complete.");
}

EditorMainWindow::~EditorMainWindow() {
}

void EditorMainWindow::onImportModel() {
    if (!mModelImporter) {
        qCritical("Model importer is null.");
        return;
    }
    QString filePath = QFileDialog::getOpenFileName(
        this,
        tr("Import 3D Model"),
        "",
        tr("3D Models (*.obj *.fbx *.gltf *.glb *.dae *.blend *.3ds);;All Files (*)")
    );

    if (!filePath.isEmpty()) {
        qInfo() << "Attempting to import model:" << filePath;
        mModelImporter->importModel(filePath);
    }
}

void EditorMainWindow::onModelImported() {
    qInfo("Model import successful, refreshing scene tree.");
    if (SceneTree) {
        SceneTree->refreshSceneTree();
    }
    if (ObjectTransformEditor) ObjectTransformEditor->setCurrentObject(INVALID_ENTITY);
    if (ObjectTextureEditor) ObjectTextureEditor->setCurrentObject(INVALID_ENTITY);
}

void EditorMainWindow::onImportFailed(const QString &error) {
    qCritical() << "Model import failed:" << error;
    QMessageBox::critical(this, tr("Import Error"), tr("Failed to import model:\n%1").arg(error));
}

void EditorMainWindow::updateTransformFromEditor(EntityID entityId, const TransformUpdateData &data) {
    if (!mWorld) {
        qWarning("EditorMainWindow::updateTransformFromEditor - World is null.");
        return;
    }

    if (entityId == INVALID_ENTITY) {
        qWarning("EditorMainWindow::updateTransformFromEditor - Received invalid entity ID.");
        return;
    }

    TransformComponent *transform = mWorld->getComponent<TransformComponent>(entityId);

    if (transform) {
        qInfo() << "EditorMainWindow: Updating transform for Entity" << entityId
                << " Pos:" << data.position << " Rot (Euler):" << data.rotation.toEulerAngles() << " Scale:" << data.
                scale;

        transform->setPosition(data.position);
        transform->setRotation(data.rotation);
        transform->setScale(data.scale);
    } else {
        qWarning() << "EditorMainWindow::updateTransformFromEditor - Could not find TransformComponent for entity ID:"
                << entityId;
        if (ObjectTransformEditor) ObjectTransformEditor->setCurrentObject(INVALID_ENTITY);
        if (ObjectTextureEditor) ObjectTextureEditor->setCurrentObject(INVALID_ENTITY);
    }
}

void EditorMainWindow::onTextureChanged(EntityID entityId, TextureType type, const QString &newPath) {
    if (!mWorld || !mResourceManager) {
        qWarning("EditorMainWindow::onTextureChanged - World or ResourceManager is null.");
        return;
    }
    if (entityId == INVALID_ENTITY) {
        qWarning("EditorMainWindow::onTextureChanged - Invalid entity ID.");
        return;
    }

    MaterialComponent *matComp = mWorld->getComponent<MaterialComponent>(entityId);
    if (!matComp) {
        qWarning() << "EditorMainWindow::onTextureChanged - Entity" << entityId << "does not have a MaterialComponent.";
        if (ObjectTextureEditor) ObjectTextureEditor->updateUI();
        return;
    }

    qInfo() << "EditorMainWindow: Updating texture for Entity" << entityId << "Type:" << static_cast<int>(type) <<
            "Path:" << newPath;

    MaterialComponent updatedMatComp = *matComp;
    QString oldMaterialKey = mResourceManager->generateMaterialCacheKey(matComp);

    switch (type) {
        case TextureType::Albedo: updatedMatComp.albedoMapResourceId = newPath;
            break;
        case TextureType::Normal: updatedMatComp.normalMapResourceId = newPath;
            break;
        case TextureType::MetallicRoughness: updatedMatComp.metallicRoughnessMapResourceId = newPath;
            break;
        case TextureType::AmbientOcclusion: updatedMatComp.ambientOcclusionMapResourceId = newPath;
            break;
        case TextureType::Emissive: updatedMatComp.emissiveMapResourceId = newPath;
            break;
        default:
            qWarning() << "Unknown texture type in onTextureChanged";
            return;
    }

    QString newMaterialCacheKey = mResourceManager->generateMaterialCacheKey(&updatedMatComp);
    qInfo() << " Old Material Key:" << oldMaterialKey;
    qInfo() << " New Material Key:" << newMaterialCacheKey;

    if (!mResourceManager->getMaterialGpuData(newMaterialCacheKey)) {
        qInfo() << "New material configuration (key:" << newMaterialCacheKey << ") not found. Loading material...";
        mResourceManager->loadMaterial(newMaterialCacheKey, &updatedMatComp);
    } else {
        qInfo() << "New material configuration (key:" << newMaterialCacheKey << ") already cached.";
    }

    switch (type) {
        case TextureType::Albedo: matComp->albedoMapResourceId = newPath;
            break;
        case TextureType::Normal: matComp->normalMapResourceId = newPath;
            break;
        case TextureType::MetallicRoughness: matComp->metallicRoughnessMapResourceId = newPath;
            break;
        case TextureType::AmbientOcclusion: matComp->ambientOcclusionMapResourceId = newPath;
            break;
        case TextureType::Emissive: matComp->emissiveMapResourceId = newPath;
            break;
    }

    qInfo() << "Entity" << entityId << "MaterialComponent updated.";
}

void EditorMainWindow::onSceneInitialized() {
    if (SceneTree) {
        SceneTree->refreshSceneTree();
    } else {
        qWarning("Editor: Cannot refresh scene tree, SceneTree widget is null.");
    }
    onSceneSelectionChanged(INVALID_ENTITY);
    if (SceneTree) {
        SceneTree->clearSelection();
    }
}

void EditorMainWindow::onCreateCube() {
    if (!mWorld || !mResourceManager) {
        qWarning("Cannot create cube: World or ResourceManager is null.");
        return;
    }

    if (!mResourceManager->getMeshGpuData(BUILTIN_CUBE_MESH_ID)) {
        mResourceManager->loadMeshFromData(BUILTIN_CUBE_MESH_ID, DEFAULT_CUBE_VERTICES, DEFAULT_CUBE_INDICES);
    }

    EntityID entity = mWorld->createEntity();
    mWorld->addComponent<TransformComponent>(entity, {});
    mWorld->addComponent<MeshComponent>(entity, {});
    mWorld->addComponent<RenderableComponent>(entity, {});
    mWorld->addComponent<MaterialComponent>(entity, {{}});

    MaterialComponent *matComp = mWorld->getComponent<MaterialComponent>(entity);
    matComp->albedoMapResourceId = DEFAULT_WHITE_TEXTURE_ID;
    matComp->normalMapResourceId = DEFAULT_NORMAL_MAP_ID;
    matComp->metallicRoughnessMapResourceId = DEFAULT_METALROUGH_TEXTURE_ID;
    matComp->ambientOcclusionMapResourceId = DEFAULT_WHITE_TEXTURE_ID;
    matComp->emissiveMapResourceId = DEFAULT_BLACK_TEXTURE_ID;

    matComp->metallicFactor = 0.1f;
    matComp->roughnessFactor = 0.8f;
    matComp->albedoFactor = {1.0f, 1.0f, 1.0f};
    matComp->aoStrength = 1.0f;
    matComp->emissiveFactor = {0.0f, 0.0f, 0.0f};

    MaterialComponent *mat = mWorld->getComponent<MaterialComponent>(entity);
    mat->albedoMapResourceId = ":/img/Images/container2.png";

    if (SceneTree) {
        SceneTree->refreshSceneTree();
    }
}

void EditorMainWindow::onCreateSphere() {
}

void EditorMainWindow::onCreatePointLight() {
    if (!mWorld) return;
    EntityID entity = mWorld->createEntity();
    mWorld->addComponent<TransformComponent>(entity, {});
    mWorld->addComponent<LightComponent>(entity, {
                                             {}, LightType::Point, {1.0f, 1.0f, 1.0f}, 1.0f,
                                             {}, 1.0f, 0.09f, 0.032f,
                                             {}, {}, {}, {}
                                         });
    // mWorld->addComponent<NameComponent>(entity, {"Point Light"});

    if (SceneTree) {
        SceneTree->refreshSceneTree();
    }
    qInfo() << "Created Point Light Entity:" << entity;
}

void EditorMainWindow::onCreateDirectionalLight() {
    if (!mWorld) return;
    EntityID entity = mWorld->createEntity();
    TransformComponent tf;
    tf.setRotation(QQuaternion::fromEulerAngles(-45.0f, -45.0f, 0.0f));
    mWorld->addComponent<TransformComponent>(entity, tf);
    mWorld->addComponent<LightComponent>(entity, {
                                             {}, LightType::Directional, {1.0f, 1.0f, 1.0f}, 1.0f,
                                             {}, 1, 0, 0, {},
                                             {0.2f, 0.2f, 0.2f}, {0.5f, 0.5f, 0.5f},
                                             {1.0f, 1.0f, 1.0f}
                                         });
    // mWorld->addComponent<NameComponent>(entity, {"Directional Light"});
    if (SceneTree) {
        SceneTree->refreshSceneTree();
    }
    qInfo() << "Created Directional Light Entity:" << entity;
}

void EditorMainWindow::onSceneSelectionChanged(EntityID entityId) {
    qDebug() << "EditorMainWindow::onSceneSelectionChanged - EntityID:" << entityId;

    bool hasTransform = false;
    bool isMeshMaterialObject = false;
    bool isLightObject = false;

    if (mWorld && entityId != INVALID_ENTITY) {
        hasTransform = mWorld->hasComponent<TransformComponent>(entityId);
        isLightObject = mWorld->hasComponent<LightComponent>(entityId);

        if (!isLightObject && hasTransform) {
            isMeshMaterialObject = mWorld->hasComponent<MeshComponent>(entityId) &&
                                   mWorld->hasComponent<MaterialComponent>(entityId);
            if (isMeshMaterialObject) {
                qDebug() << "  Entity" << entityId << "is eligible for Material Editor.";
            }
        } else if (isLightObject) {
             qDebug() << "  Entity" << entityId << "is eligible for Light Editor.";
        }
    } else {
         qDebug() << "  Invalid entity ID or null World.";
    }

    if (ObjectTransformEditor) {
        ObjectTransformEditor->setCurrentObject(hasTransform ? entityId : INVALID_ENTITY);
        ObjectTransformEditor->setEnabled(hasTransform);
        qDebug() << "  Transform Editor Set Enabled:" << hasTransform;
    }

    if (ObjectTextureEditor && TextureDock) {
        ObjectTextureEditor->setCurrentObject(isMeshMaterialObject ? entityId : INVALID_ENTITY);
        TextureDock->setVisible(isMeshMaterialObject);
        qDebug() << "  Texture Dock Set Visible:" << isMeshMaterialObject;
    } else {
        qWarning("ObjectTextureEditor or TextureDock is null!");
    }

    if (mLightEditor && mLightDock) {
        mLightEditor->setCurrentObject(isLightObject ? entityId : INVALID_ENTITY);
        mLightDock->setVisible(isLightObject);
         qDebug() << "  Light Dock Set Visible:" << isLightObject;
    } else {
         qWarning("mLightEditor or mLightDock is null!");
    }
}

void EditorMainWindow::setupModelImporter() {
    if (!mWorld || !mResourceManager) {
        qWarning("Cannot setup ModelImporter: World or ResourceManager is null.");
        return;
    }
    mModelImporter = QSharedPointer<ModelImporter>::create(mWorld, mResourceManager, this); // Parent to main window
    connect(mModelImporter.get(), &ModelImporter::modelImportedSuccessfully, this, &EditorMainWindow::onModelImported);
    connect(mModelImporter.get(), &ModelImporter::importFailed, this, &EditorMainWindow::onImportFailed);
}

void EditorMainWindow::InitEdgeLayout() {
    // 初始化状态栏(底部信息)
    BottomStatusBar = new EditorStatusBar(this);
    this->setStatusBar(BottomStatusBar);

    // 创建工具栏
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
    auto *rightDock = new EditorDockWidget("Properties", this);
    rightDock->setObjectName("PropertiesDock");
    rightDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    rightDock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
    auto *rightPanelWidget = new QWidget(rightDock);
    auto *rightPanelLayout = new QVBoxLayout(rightPanelWidget);
    rightPanelLayout->setContentsMargins(0, 0, 0, 0);
    rightPanelWidget->setLayout(rightPanelLayout);

    auto *rightSplitter = new QSplitter(Qt::Vertical, rightPanelWidget);

    SceneTree = new SceneTreeWidget(rightSplitter);

    QWidget *transformTextureContainer = new QWidget(rightSplitter);
    QVBoxLayout *transformTextureLayout = new QVBoxLayout(transformTextureContainer);
    transformTextureLayout->setContentsMargins(0, 0, 0, 0);
    transformTextureContainer->setLayout(transformTextureLayout);

    ObjectTransformEditor = new TransformEditor(transformTextureContainer);
    TextureDock = new EditorDockWidget("Material", transformTextureContainer);
    TextureDock->setObjectName("TextureEditorDock");
    TextureDock->setFeatures(QDockWidget::NoDockWidgetFeatures);
    ObjectTextureEditor = new TextureEditor(TextureDock);
    TextureDock->setWidget(ObjectTextureEditor);

    transformTextureLayout->addWidget(ObjectTransformEditor);
    transformTextureLayout->addWidget(TextureDock);
    transformTextureLayout->setStretchFactor(ObjectTransformEditor, 0);
    transformTextureLayout->setStretchFactor(TextureDock, 1);

    rightSplitter->addWidget(SceneTree);
    rightSplitter->addWidget(transformTextureContainer);
    rightSplitter->setStretchFactor(0, 1);
    rightSplitter->setStretchFactor(1, 2);

    rightPanelLayout->addWidget(rightSplitter);
    rightDock->setWidget(rightPanelWidget);

    mLightDock = new EditorDockWidget("Light", this);
    mLightDock->setObjectName("LightEditorDock");
    mLightDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    mLightDock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
    mLightEditor = new LightEditor(mLightDock);
    mLightDock->setWidget(mLightEditor);

    this->addDockWidget(Qt::RightDockWidgetArea, rightDock);
    this->addDockWidget(Qt::RightDockWidgetArea, mLightDock);

    TextureDock->setVisible(false);
    mLightDock->setVisible(false);

    resizeDocks({rightDock, mLightDock}, {350, 200}, Qt::Horizontal);
}

void EditorMainWindow::InitContent() {
    ViewCentralWidget = new ViewRenderWidget();

    setCentralWidget(ViewCentralWidget);
}
