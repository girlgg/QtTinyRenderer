#pragma once

#include "Component/Component.h"

struct TransformComponent;

struct CameraControllerComponent : Component {
    float mMoveSpeed = 0.01f;
    float mRotateSpeed = 1.f;

    float mYaw = 0.0f;
    float mPitch = 0.0f;
    float mMinPitch = -89.0f;
    float mMaxPitch = 89.0f;

    bool mFirstUpdate = true;
};
