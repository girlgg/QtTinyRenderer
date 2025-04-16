#include "System/CameraSystem.h"

#include "Component/CameraComponent.h"
#include "Component/CameraControllerComponent.h"
#include "Component/TransformComponent.h"
#include "Scene/World.h"
#include "System/InputSystem.h"

void CameraSystem::update(World *world, float deltaTime) {
    auto view = world->view<TransformComponent, CameraComponent, CameraControllerComponent>();

    for (EntityID entity: view) {
        auto transform = world->getComponent<TransformComponent>(entity);
        auto controller = world->getComponent<CameraControllerComponent>(entity);

        if (!transform) return;

        QVector3D movement;
        if (InputSystem::get().getKey(Qt::Key_W)) {
            movement -= transform->forward();
        }
        if (InputSystem::get().getKey(Qt::Key_S)) {
            movement += transform->forward();
        }
        if (InputSystem::get().getKey(Qt::Key_A)) {
            movement -= transform->right();
        }
        if (InputSystem::get().getKey(Qt::Key_D)) {
            movement += transform->right();
        }
        if (InputSystem::get().getKey(Qt::Key_E)) {
            movement -= transform->up();
        }
        if (InputSystem::get().getKey(Qt::Key_Q)) {
            movement += transform->up();
        }

        if (!movement.isNull()) {
            transform->translate(movement.normalized() * controller->mMoveSpeed * deltaTime);
        }

        if (InputSystem::get().isMouseCaptured()) {
            if (controller->mFirstUpdate) {
                QVector3D fwd = transform->forward();

                controller->mYaw = std::atan2(fwd.x(), fwd.z()) * 180.0f / M_PI;
                controller->mPitch = std::asin(fwd.y()) * 180.0f / M_PI;

                controller->mFirstUpdate = false;
            }
            QPointF delta = InputSystem::get().consumeMouseDelta();

            controller->mYaw -= delta.x() * controller->mRotateSpeed;
            controller->mPitch -= delta.y() * controller->mRotateSpeed;

            controller->mPitch = std::clamp(controller->mPitch, controller->mMinPitch, controller->mMaxPitch);

            QQuaternion yawRot = QQuaternion::fromAxisAndAngle(0, 1, 0, controller->mYaw);
            QQuaternion pitchRot = QQuaternion::fromAxisAndAngle(1, 0, 0, controller->mPitch);

            QQuaternion finalRot = yawRot * pitchRot;

            transform->setRotation(finalRot);
        }
    }
}
