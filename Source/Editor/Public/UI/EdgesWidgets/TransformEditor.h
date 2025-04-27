#pragma once

#include <QWidget>
#include <QVector3D>
#include <math3d/qquaternion.h>

#include "ECSCore.h"

class World;
QT_BEGIN_NAMESPACE

namespace Ui {
    class TransformEditor;
}

QT_END_NAMESPACE

struct TransformUpdateData {
    QVector3D position;
    QQuaternion rotation;
    QVector3D scale;
};

Q_DECLARE_METATYPE(TransformUpdateData);

class TransformEditor : public QWidget {
    Q_OBJECT

public:
    explicit TransformEditor(QWidget *parent = nullptr);

    ~TransformEditor() override;

    void setWorld(QSharedPointer<World> world);

public slots:
    void setCurrentObject(EntityID objId);
private slots:
    void updateTransform();
    void updateFromSceneManager();

signals:
    void transformChanged(EntityID objId, const TransformUpdateData& data);


private:
    void setupConnections();

    void blockUIUpdates(bool block);

    Ui::TransformEditor *ui;
    QSharedPointer<World> mWorld;
    int mCurrentObjId = INVALID_ENTITY;
    bool mUpdatingUI = false;
};
