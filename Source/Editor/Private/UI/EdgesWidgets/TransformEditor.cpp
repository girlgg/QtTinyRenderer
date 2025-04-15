#include <QLineEdit>
#include <QVBoxLayout>
#include <QLabel>

#include "UI/EdgesWidgets/TransformEditor.h"

#include <util/qvalidator.h>

#include "Scene/SceneManager.h"
#include "UI/EdgesWidgets/ui_TransformEditor.h"

#define CONNECT_UPDATE_TRANSFORM(UIWIDGET) connect(UIWIDGET, &QLineEdit::textChanged, this, &TransformEditor::updateTransform);

TransformEditor::TransformEditor(QWidget *parent) : QWidget(parent), ui(new Ui::TransformEditor) {
    ui->setupUi(this);

    setupConnections();
}

TransformEditor::~TransformEditor() {
    delete ui;
}

void TransformEditor::setCurrentObject(int objId) {
    if (m_currentObjId == objId) return;
    m_currentObjId = objId;
    updateFromSceneManager();
}

void TransformEditor::updateFromSceneManager() {
    if (m_currentObjId == -1) {
        ui->lineEditTranslationX->clear();
        ui->lineEditTranslationY->clear();
        ui->lineEditTranslationZ->clear();
        return;
    }

    const auto loc = SceneManager::get().getObjectLocation(m_currentObjId);
    blockUIUpdates(true);

    // 根据数值大小自动选择显示格式
    auto formatNumber = [](float val) {
        return qAbs(val) >= 1e4 || qAbs(val) <= 1e-4 ?
               QString::number(val, 'g', 6) :
               QString::number(val, 'f', 4);
    };

    ui->lineEditTranslationX->setText(formatNumber(loc.x()));
    ui->lineEditTranslationY->setText(formatNumber(loc.x()));
    ui->lineEditTranslationZ->setText(formatNumber(loc.x()));

    blockUIUpdates(false);
}

void TransformEditor::updateTransform() {
    if (m_currentObjId == -1) return;

    bool ok;
    const float x = ui->lineEditTranslationX->text().toFloat(&ok);
    if (!ok) return;
    const float y = ui->lineEditTranslationY->text().toFloat(&ok);
    if (!ok) return;
    const float z = ui->lineEditTranslationZ->text().toFloat(&ok);
    if (!ok) return;

    emit transformChanged(m_currentObjId, QVector3D(x, y, z));
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
}
