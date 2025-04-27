#include "UI/EdgesWidgets/LightEditor.h"

#include "Component/LightComponent.h"
#include "Scene/World.h"
#include "UI/EdgesWidgets/ui_LightEditor.h"
#include <QColorDialog>

LightEditor::LightEditor(QWidget *parent) : QWidget(parent), ui(new Ui::LightEditor) {
    ui->setupUi(this);
    setEnabled(false);
    ui->groupBoxPointLight->setVisible(false);
    ui->groupBoxDirectionalLight->setVisible(false);
    setupConnections();
}

LightEditor::~LightEditor() {
    delete ui;
}

void LightEditor::setWorld(QSharedPointer<World> world) {
    mWorld = world;
}

void LightEditor::setCurrentObject(EntityID objId) {
    mCurrentObjId = objId;
    updateUI();
}

void LightEditor::updateLightProperties() {
    if (mUpdatingUI || mCurrentObjId == INVALID_ENTITY || !mWorld) return;

    LightComponent *lc = mWorld->getComponent<LightComponent>(mCurrentObjId);
    if (!lc) return;

    switch (lc->type) {
        case LightType::Point: {
            lc->intensity = ui->doubleSpinBoxPointIntensity->value();
            lc->constantAttenuation = ui->doubleSpinBoxPointConstant->value();
            lc->linearAttenuation = ui->doubleSpinBoxPointLinear->value();
            lc->quadraticAttenuation = ui->doubleSpinBoxPointQuadratic->value();
            QColor uiColor = getColorFromButton(ui->buttonColorPoint);
            if (uiColor.isValid()) {
                lc->color = QVector3D(uiColor.redF(), uiColor.greenF(), uiColor.blueF());
            }
        }
        break;
        case LightType::Directional: {
            lc->intensity = ui->doubleSpinBoxDirIntensity->value();
            QColor uiColor = getColorFromButton(ui->buttonColorDir);
            if (uiColor.isValid()) {
                lc->color = QVector3D(uiColor.redF(), uiColor.greenF(), uiColor.blueF());
            }
        }
        break;
    }
}

void LightEditor::onSelectPointColor() {
    if (mCurrentObjId == INVALID_ENTITY || !mWorld) return;
    LightComponent *lc = mWorld->getComponent<LightComponent>(mCurrentObjId);
    if (!lc || lc->type != LightType::Point) return;

    QColor currentColor = QColor::fromRgbF(lc->color.x(), lc->color.y(), lc->color.z());

    const QColor newColor = QColorDialog::getColor(currentColor, this, "Select Point Light Color");

    if (newColor.isValid()) {
        updateButtonColor(ui->buttonColorPoint, newColor);
        updateLightProperties();
    }
}

void LightEditor::onSelectDirectionalColor() {
    if (mCurrentObjId == INVALID_ENTITY || !mWorld) return;
    LightComponent *lc = mWorld->getComponent<LightComponent>(mCurrentObjId);
    if (!lc || lc->type != LightType::Directional) return;

    QColor currentColor = QColor::fromRgbF(lc->color.x(), lc->color.y(), lc->color.z());

    const QColor newColor = QColorDialog::getColor(currentColor, this, "Select Directional Light Color");

    if (newColor.isValid()) {
        updateButtonColor(ui->buttonColorDir, newColor);
        updateLightProperties();
    }
}

void LightEditor::setupConnections() {
    connect(ui->doubleSpinBoxPointIntensity, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
            &LightEditor::updateLightProperties);
    connect(ui->doubleSpinBoxDirIntensity, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
            &LightEditor::updateLightProperties);

    connect(ui->doubleSpinBoxPointConstant, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
            &LightEditor::updateLightProperties);
    connect(ui->doubleSpinBoxPointLinear, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
            &LightEditor::updateLightProperties);
    connect(ui->doubleSpinBoxPointQuadratic, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
            &LightEditor::updateLightProperties);

    connect(ui->buttonColorPoint, &QPushButton::clicked, this, &LightEditor::onSelectPointColor);
    connect(ui->buttonColorDir, &QPushButton::clicked, this, &LightEditor::onSelectDirectionalColor);
}

void LightEditor::updateUI() {
    if (mUpdatingUI) return;
    mUpdatingUI = true;
    blockUIUpdates(true);

    bool pointVisible = false;
    bool dirVisible = false;
    setEnabled(false);

    updateButtonColor(ui->buttonColorPoint, QColor());
    updateButtonColor(ui->buttonColorDir, QColor());

    if (mCurrentObjId != INVALID_ENTITY && mWorld) {
        LightComponent *lc = mWorld->getComponent<LightComponent>(mCurrentObjId);
        if (lc) {
            setEnabled(true);
            QColor currentLightColor = QColor::fromRgbF(lc->color.x(), lc->color.y(), lc->color.z());
            switch (lc->type) {
                case LightType::Point:
                    pointVisible = true;
                    ui->doubleSpinBoxPointIntensity->setValue(lc->intensity);
                    ui->doubleSpinBoxPointConstant->setValue(lc->constantAttenuation);
                    ui->doubleSpinBoxPointLinear->setValue(lc->linearAttenuation);
                    ui->doubleSpinBoxPointQuadratic->setValue(lc->quadraticAttenuation);
                    updateButtonColor(ui->buttonColorPoint, currentLightColor);
                    break;
                case LightType::Directional:
                    dirVisible = true;
                    ui->doubleSpinBoxDirIntensity->setValue(lc->intensity);
                    updateButtonColor(ui->buttonColorDir, currentLightColor);
                    break;
            }
        }
    }

    ui->groupBoxPointLight->setVisible(pointVisible);
    ui->groupBoxDirectionalLight->setVisible(dirVisible);

    if (!isEnabled()) {
        ui->doubleSpinBoxPointIntensity->setValue(0);
        ui->doubleSpinBoxPointConstant->setValue(1.0);
        ui->doubleSpinBoxPointLinear->setValue(0.09);
        ui->doubleSpinBoxPointQuadratic->setValue(0.032);
        ui->doubleSpinBoxDirIntensity->setValue(0);
    }

    blockUIUpdates(false);
    mUpdatingUI = false;
}

void LightEditor::blockUIUpdates(bool block) {
    ui->doubleSpinBoxPointIntensity->blockSignals(block);
    ui->doubleSpinBoxDirIntensity->blockSignals(block);

    ui->doubleSpinBoxPointConstant->blockSignals(block);
    ui->doubleSpinBoxPointLinear->blockSignals(block);
    ui->doubleSpinBoxPointQuadratic->blockSignals(block);

    ui->buttonColorPoint->blockSignals(block);
    ui->buttonColorDir->blockSignals(block);
}

void LightEditor::updateButtonColor(QPushButton *button, const QColor &color) {
    if (!button) return;
    button->setAutoFillBackground(true);
    QPalette pal = button->palette();
    pal.setColor(QPalette::Button, color.isValid() ? color : QColor(Qt::white));
    button->setPalette(pal);
    button->update();
}

QColor LightEditor::getColorFromButton(QPushButton *button) const {
    if (!button) return QColor();
    return button->palette().color(QPalette::Button);
}
