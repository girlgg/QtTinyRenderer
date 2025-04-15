#include "Component/CameraComponent.h"
#include "Component/TransformComponent.h"

QMatrix4x4 CameraComponent::getProjectionMatrix() const {
    QMatrix4x4 proj;
    proj.perspective(mFov, mAspect, mNearPlane, mFarPlane);
    return proj;
}
