#include "Component/CameraControllerComponent.h"
#include "Scene/InputSystem.h"
#include "Component/TransformComponent.h"

void CameraControllerComponent::update(float deltaTime, TransformComponent* transform) {
    if (!transform) return;

    QVector3D movement;
    if (InputSystem::get().getKey(Qt::Key_W)) {
        movement += transform->forward();
    }
    if (InputSystem::get().getKey(Qt::Key_S)) {
        movement -= transform->forward();
    }
    if (InputSystem::get().getKey(Qt::Key_A)) {
        movement += transform->right();
    }
    if (InputSystem::get().getKey(Qt::Key_D)) {
        movement -= transform->right();
    }
    if (InputSystem::get().getKey(Qt::Key_E)) {
        movement += transform->up();
    }
    if (InputSystem::get().getKey(Qt::Key_Q)) {
        movement -= transform->up();
    }

    if (!movement.isNull()) {
        transform->translate(movement.normalized() * mMoveSpeed * deltaTime);
    }

    if (InputSystem::get().isMouseCaptured()) {
        if (mFirstUpdate) {
            QVector3D fwd = transform->forward();

            mYaw = std::atan2(fwd.x(), fwd.z()) * 180.0f / M_PI;
            mPitch = std::asin(fwd.y()) * 180.0f / M_PI;

            mFirstUpdate = false;
        }
        QPointF delta = InputSystem::get().consumeMouseDelta();

        mYaw -= delta.x() * mRotateSpeed;
        mPitch += delta.y() * mRotateSpeed;

        mPitch = std::clamp(mPitch, mMinPitch, mMaxPitch);

        QQuaternion yawRot = QQuaternion::fromAxisAndAngle(0, 1, 0, mYaw);
        QQuaternion pitchRot = QQuaternion::fromAxisAndAngle(1, 0, 0, mPitch);

        QQuaternion finalRot = yawRot * pitchRot;

        transform->setRotation(finalRot);
    }
}
