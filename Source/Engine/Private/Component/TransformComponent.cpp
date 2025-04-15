#include "Component/TransformComponent.h"

QMatrix4x4 TransformComponent::worldMatrix() const {
    QMatrix4x4 matrix;
    matrix.translate(mPosition);
    matrix.rotate(mRotation);
    matrix.scale(mScale);
    return matrix;
}

QMatrix3x3 TransformComponent::normalMatrix() const {
    return worldMatrix().normalMatrix();
}

void TransformComponent::translate(const QVector3D &translation) {
    setPosition(position() + translation);
}

void TransformComponent::translate(float x, float y, float z) {
    translate(QVector3D(x, y, z));
}

void TransformComponent::rotate(const QQuaternion &quaternion) {
    setRotation(rotation() * quaternion);
}

void TransformComponent::rotate(float angle, const QVector3D &axis) {
    rotate(QQuaternion::fromAxisAndAngle(axis, angle));
}

void TransformComponent::rotate(float angle, float x, float y, float z) {
    rotate(angle, QVector3D(x, y, z));
}

void TransformComponent::scale(const QVector3D &scaling) {
    setScale(scale() * scaling);
}

void TransformComponent::scale(float factor) {
    scale(QVector3D(factor, factor, factor));
}

void TransformComponent::setPosition(const QVector3D &pos) {
    if (mPosition != pos) {
        mPosition = pos;
    }
}

void TransformComponent::setRotation(const QQuaternion &rot) {
    if (mRotation != rot) {
        mRotation = rot;
    }
}

QVector3D TransformComponent::forward() const {
    return mRotation.rotatedVector(QVector3D(0, 0, 1)).normalized();
}

QVector3D TransformComponent::up() const {
    return mRotation.rotatedVector(QVector3D(0, 1, 0)).normalized();
}

QVector3D TransformComponent::right() const {
    return mRotation.rotatedVector(QVector3D(1, 0, 0)).normalized();
}

QMatrix4x4 TransformComponent::getViewMatrix() {
    QMatrix4x4 view;
    view.lookAt(position(), position() + forward(), up());
    return view;
}

void TransformComponent::setScale(const QVector3D &scl) {
    if (mScale != scl) {
        mScale = scl;
    }
}
