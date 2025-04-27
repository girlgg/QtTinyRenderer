#include <QLineEdit>
#include <QVBoxLayout>
#include <QLabel>

#include "UI/EdgesWidgets/TransformEditor.h"

#include <util/qvalidator.h>

#include "Component/TransformComponent.h"
#include "Scene/World.h"
#include "UI/EdgesWidgets/ui_TransformEditor.h"

#define CONNECT_UPDATE_TRANSFORM(UIWIDGET) connect(UIWIDGET, &QLineEdit::editingFinished, this, &TransformEditor::updateTransform);\
connect(UIWIDGET, &QLineEdit::returnPressed, this, &TransformEditor::updateTransform)

float safeStringToFloat(const QString& str, bool* ok = nullptr) {
    bool conversionOk;
    float val = str.toFloat(&conversionOk);
    if (ok) *ok = conversionOk;
    if (!conversionOk || std::isnan(val) || std::isinf(val)) {
        if (ok) *ok = false;
        return 0.0f;
    }
    return val;
}

TransformEditor::TransformEditor(QWidget *parent) : QWidget(parent), ui(new Ui::TransformEditor) {
    ui->setupUi(this);
    qRegisterMetaType<TransformUpdateData>("TransformUpdateData");
    setupConnections();
    setEnabled(false);
}

TransformEditor::~TransformEditor() {
    delete ui;
}

void TransformEditor::setCurrentObject(EntityID objId) {
    if (mCurrentObjId == objId) return;

    qDebug() << "TransformEditor::setCurrentObject - ID:" << objId;
    mCurrentObjId = objId;

    if (mCurrentObjId == INVALID_ENTITY || !mWorld) {
        setEnabled(false);
        updateFromSceneManager();
    } else {
        TransformComponent* transform = mWorld->getComponent<TransformComponent>(mCurrentObjId);
        if (transform) {
            setEnabled(true);
            updateFromSceneManager();
        } else {
            qWarning() << "Selected entity" << objId << "has no TransformComponent.";
            setEnabled(false);
            mCurrentObjId = INVALID_ENTITY;
            updateFromSceneManager();
        }
    }

    if (mCurrentObjId == objId) return;
    mCurrentObjId = objId;
    updateFromSceneManager();
}

void TransformEditor::setWorld(QSharedPointer<World> world) {
    mWorld = world;
}

void TransformEditor::updateFromSceneManager() {
    // 防止递归调用
    if (mUpdatingUI) return;
    mUpdatingUI = true;

    blockUIUpdates(true);
    if (mCurrentObjId == INVALID_ENTITY || !mWorld) {
        ui->lineEditTranslationX->clear();
        ui->lineEditTranslationY->clear();
        ui->lineEditTranslationZ->clear();
        ui->lineEditRotationX->clear();
        ui->lineEditRotationY->clear();
        ui->lineEditRotationZ->clear();
        ui->lineEditScaleX->clear();
        ui->lineEditScaleY->clear();
        ui->lineEditScaleZ->clear();
        setEnabled(false);
    } else {
        TransformComponent *transform = mWorld->getComponent<TransformComponent>(mCurrentObjId);
        if (transform) {
            setEnabled(true);
            QVector3D pos = transform->position();
            QQuaternion rot = transform->rotation();
            QVector3D scale = transform->scale();
            QVector3D euler = rot.toEulerAngles();

            auto formatNumber = [](float val) {
                 return QString::number(static_cast<double>(val), 'f', 3);
            };

            ui->lineEditTranslationX->setText(formatNumber(pos.x()));
            ui->lineEditTranslationY->setText(formatNumber(pos.y()));
            ui->lineEditTranslationZ->setText(formatNumber(pos.z()));

            ui->lineEditRotationX->setText(formatNumber(euler.x()));
            ui->lineEditRotationY->setText(formatNumber(euler.y()));
            ui->lineEditRotationZ->setText(formatNumber(euler.z()));

            ui->lineEditScaleX->setText(formatNumber(scale.x()));
            ui->lineEditScaleY->setText(formatNumber(scale.y()));
            ui->lineEditScaleZ->setText(formatNumber(scale.z()));
        } else {
             qWarning() << "updateFromSceneManager: TransformComponent lost for entity" << mCurrentObjId;
             setCurrentObject(INVALID_ENTITY);
        }
    }

    blockUIUpdates(false);
    mUpdatingUI = false;
}

void TransformEditor::updateTransform() {
    if (mUpdatingUI) return;
    if (mCurrentObjId == INVALID_ENTITY || !mWorld) return;

    qDebug() << "TransformEditor::updateTransform triggered for Entity:" << mCurrentObjId;

    bool ok = true;
    const float posX = safeStringToFloat(ui->lineEditTranslationX->text(), &ok); if (!ok) return;
    const float posY = safeStringToFloat(ui->lineEditTranslationY->text(), &ok); if (!ok) return;
    const float posZ = safeStringToFloat(ui->lineEditTranslationZ->text(), &ok); if (!ok) return;

    const float rotX = safeStringToFloat(ui->lineEditRotationX->text(), &ok); if (!ok) return;
    const float rotY = safeStringToFloat(ui->lineEditRotationY->text(), &ok); if (!ok) return;
    const float rotZ = safeStringToFloat(ui->lineEditRotationZ->text(), &ok); if (!ok) return;

    const float scaleX = safeStringToFloat(ui->lineEditScaleX->text(), &ok); if (!ok) return;
    const float scaleY = safeStringToFloat(ui->lineEditScaleY->text(), &ok); if (!ok) return;
    const float scaleZ = safeStringToFloat(ui->lineEditScaleZ->text(), &ok); if (!ok) return;

    TransformUpdateData updateData;
    updateData.position = QVector3D(posX, posY, posZ);
    updateData.rotation = QQuaternion::fromEulerAngles(rotX, rotY, rotZ);
    updateData.scale = QVector3D(scaleX, scaleY, scaleZ);

    emit transformChanged(mCurrentObjId, updateData);
}

void TransformEditor::setupConnections() {
    CONNECT_UPDATE_TRANSFORM(ui->lineEditTranslationX);
    CONNECT_UPDATE_TRANSFORM(ui->lineEditTranslationY);
    CONNECT_UPDATE_TRANSFORM(ui->lineEditTranslationZ);
    CONNECT_UPDATE_TRANSFORM(ui->lineEditRotationX);
    CONNECT_UPDATE_TRANSFORM(ui->lineEditRotationY);
    CONNECT_UPDATE_TRANSFORM(ui->lineEditRotationZ);
    CONNECT_UPDATE_TRANSFORM(ui->lineEditScaleX);
    CONNECT_UPDATE_TRANSFORM(ui->lineEditScaleY);
    CONNECT_UPDATE_TRANSFORM(ui->lineEditScaleZ);
}

void TransformEditor::blockUIUpdates(bool block) {
    ui->lineEditTranslationX->blockSignals(block);
    ui->lineEditTranslationY->blockSignals(block);
    ui->lineEditTranslationZ->blockSignals(block);
    ui->lineEditRotationX->blockSignals(block);
    ui->lineEditRotationY->blockSignals(block);
    ui->lineEditRotationZ->blockSignals(block);
    ui->lineEditScaleX->blockSignals(block);
    ui->lineEditScaleY->blockSignals(block);
    ui->lineEditScaleZ->blockSignals(block);
}
