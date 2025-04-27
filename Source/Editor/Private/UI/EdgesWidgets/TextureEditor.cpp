#include "UI/EdgesWidgets/TextureEditor.h"

#include <QFileDialog>
#include <QLineEdit>

#include "Component/MaterialComponent.h"
#include "Component/MeshComponent.h"
#include "Scene/World.h"
#include "UI/EdgesWidgets/ui_TextureEditor.h"
#include <QPushButton>

#include "CommonType.h"

#define CONNECT_BROWSE_BUTTON(TEXTURE_NAME) \
connect(ui->buttonBrowse##TEXTURE_NAME, &QPushButton::clicked, this, &TextureEditor::onBrowse##TEXTURE_NAME)

TextureEditor::TextureEditor(QWidget *parent) : QWidget(parent), ui(new Ui::TextureEditor) {
    ui->setupUi(this);
    qRegisterMetaType<TextureType>("TextureType");
    setEnabled(false);
    setupConnections();
}

TextureEditor::~TextureEditor() {
    delete ui;
}

void TextureEditor::setWorld(QSharedPointer<World> world) {
    mWorld = world;
}

void TextureEditor::setResourceManager(QSharedPointer<ResourceManager> resourceManager) {
    mResourceManager = resourceManager;
}

void TextureEditor::setCurrentObject(EntityID objId) {
    qDebug() << "TextureEditor::setCurrentObject - New ID:" << objId << " (Previous ID: " << mCurrentObjId << ")";
    mCurrentObjId = objId;

    if (!mWorld) {
        qWarning() << "TextureEditor::setCurrentObject - World is null!";
        setEnabled(false);
        updateUI(); // 即使 world 为空也要清理 UI
        return;
    }

    if (mCurrentObjId == INVALID_ENTITY) {
        setEnabled(false);
    } else {
        bool hasMesh = mWorld->hasComponent<MeshComponent>(mCurrentObjId);
        bool hasMaterial = mWorld->hasComponent<MaterialComponent>(mCurrentObjId);

        if (hasMesh && hasMaterial) {
            qDebug() << "TextureEditor: Entity" << objId << "has Mesh and Material. Enabling editor.";
            setEnabled(true);
        } else {
            qDebug() << "TextureEditor: Entity" << objId << "missing Mesh or Material. Disabling editor. HasMesh:" <<
                    hasMesh << "HasMaterial:" << hasMaterial;
            setEnabled(false);
        }
    }
    updateUI();
}

void TextureEditor::updateUI() {
    if (mUpdatingUI) return;
    mUpdatingUI = true;
    blockUIUpdates(true); // Prevent signals during update

    if (mCurrentObjId != INVALID_ENTITY && mWorld && mWorld->hasComponent<MaterialComponent>(mCurrentObjId)) {
        MaterialComponent *matComp = mWorld->getComponent<MaterialComponent>(mCurrentObjId);
        if (matComp) {
            ui->lineEditAlbedoPath->setText(matComp->albedoMapResourceId);
            ui->lineEditNormalPath->setText(matComp->normalMapResourceId);
            ui->lineEditMetallicRoughnessPath->setText(matComp->metallicRoughnessMapResourceId);
            ui->lineEditAOPath->setText(matComp->ambientOcclusionMapResourceId);
            ui->lineEditEmissivePath->setText(matComp->emissiveMapResourceId);
            setEnabled(true);
        } else {
            qWarning() << "TextureEditor::updateUI - Could not get MaterialComponent for valid ID:" << mCurrentObjId;
            ui->lineEditAlbedoPath->clear();
            ui->lineEditNormalPath->clear();
            ui->lineEditMetallicRoughnessPath->clear();
            ui->lineEditAOPath->clear();
            ui->lineEditEmissivePath->clear();
            setEnabled(false);
        }
    } else {
        ui->lineEditAlbedoPath->clear();
        ui->lineEditNormalPath->clear();
        ui->lineEditMetallicRoughnessPath->clear();
        ui->lineEditAOPath->clear();
        ui->lineEditEmissivePath->clear();
        setEnabled(false);
    }

    blockUIUpdates(false);
    mUpdatingUI = false;
}

void TextureEditor::onBrowseAlbedo() {
    browseForTexture(TextureType::Albedo, ui->lineEditAlbedoPath);
}

void TextureEditor::onBrowseNormal() {
    browseForTexture(TextureType::Normal, ui->lineEditNormalPath);
}

void TextureEditor::onBrowseMetallicRoughness() {
    browseForTexture(TextureType::MetallicRoughness, ui->lineEditMetallicRoughnessPath);
}

void TextureEditor::onBrowseAO() {
    browseForTexture(TextureType::AmbientOcclusion, ui->lineEditAOPath);
}

void TextureEditor::onBrowseEmissive() {
    browseForTexture(TextureType::Emissive, ui->lineEditEmissivePath);
}

void TextureEditor::setupConnections() {
    CONNECT_BROWSE_BUTTON(Albedo);
    CONNECT_BROWSE_BUTTON(Normal);
    CONNECT_BROWSE_BUTTON(MetallicRoughness);
    CONNECT_BROWSE_BUTTON(AO);
    CONNECT_BROWSE_BUTTON(Emissive);
}

void TextureEditor::blockUIUpdates(bool block) {
}

void TextureEditor::browseForTexture(TextureType type, QLineEdit *displayLineEdit) {
    if (mCurrentObjId == INVALID_ENTITY) return;

    QString currentPath = displayLineEdit->text();
    QString initialDir = QFileInfo(currentPath).absolutePath();
    if (initialDir.isEmpty()) {
        initialDir = QDir::currentPath();
    }


    QString filePath = QFileDialog::getOpenFileName(
        this,
        tr("Select Texture"),
        initialDir,
        tr("Images (*.png *.jpg *.jpeg *.bmp *.tga *.dds);;All Files (*)")
    );

    if (!filePath.isEmpty()) {
        qDebug() << "TextureEditor: Selected new path for" << static_cast<int>(type) << ":" << filePath;
        displayLineEdit->setText(filePath);
        emit textureChanged(mCurrentObjId, type, filePath);
    }
}
