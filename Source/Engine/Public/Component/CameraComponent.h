#pragma once

#include <QMatrix4x4>
#include "Component/Component.h"

struct CameraComponent : Component {
    QMatrix4x4 getProjectionMatrix() const;

    float mAspect = 1.2f;
    float mFov = 90.f;
    float mNearPlane = 0.1f;
    float mFarPlane = 1000.0f;
};
