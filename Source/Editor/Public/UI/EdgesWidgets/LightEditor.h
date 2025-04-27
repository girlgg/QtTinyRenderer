#pragma once

#include <QWidget>

#include "ECSCore.h"


class QPushButton;
struct LightComponent;
class World;
QT_BEGIN_NAMESPACE

namespace Ui {
    class LightEditor;
}

QT_END_NAMESPACE

class LightEditor : public QWidget {
    Q_OBJECT

public:
    explicit LightEditor(QWidget *parent = nullptr);

    ~LightEditor() override;

    void setWorld(QSharedPointer<World> world);

public slots:
    void setCurrentObject(EntityID objId);

private slots:
    void updateLightProperties();

    void onSelectPointColor();

    void onSelectDirectionalColor();

signals:
    void lightChanged(EntityID entityId, const LightComponent &data);

private:
    void setupConnections();

    void updateUI();

    void blockUIUpdates(bool block);

    void updateButtonColor(QPushButton *button, const QColor &color);

    QColor getColorFromButton(QPushButton *button) const;

    Ui::LightEditor *ui;
    QSharedPointer<World> mWorld;
    EntityID mCurrentObjId = INVALID_ENTITY;
    bool mUpdatingUI = false;
};
