#pragma once
#include <QMatrix4x4>
#include <QVector3D>
#include <kernel/qobjectdefs.h>
#include <kernel/qtmetamacros.h>
#include <math3d/qquaternion.h>

#include "Component.h"

struct TransformComponent : public Component {
public:
    QMatrix4x4 worldMatrix() const;

    QMatrix3x3 normalMatrix() const;

    QVector3D position() { return mPosition; }

    void translate(const QVector3D &translation);

    void translate(float x, float y, float z);

    void rotate(const QQuaternion &quaternion);

    void rotate(float angle, const QVector3D &axis);

    void rotate(float angle, float x, float y, float z);

    void scale(const QVector3D &scaling);

    void scale(float factor);

    void setPosition(const QVector3D &pos);

    QQuaternion rotation() const { return mRotation; }

    void setRotation(const QQuaternion &rot);

    QVector3D scale() const { return mScale; }

    void setScale(const QVector3D &scl);

    QVector3D forward() const;

    QVector3D up() const;

    QVector3D right() const;

    QMatrix4x4 getViewMatrix();

private:
    QVector3D mPosition = QVector3D(0, 0, 0);
    QQuaternion mRotation = QQuaternion::fromAxisAndAngle(QVector3D(0, 1, 0), 0);
    QVector3D mScale = QVector3D(1, 1, 1);
};
