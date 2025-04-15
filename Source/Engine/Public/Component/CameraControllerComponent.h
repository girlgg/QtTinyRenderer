#pragma once

#include "Component/Component.h"

struct TransformComponent;

struct CameraControllerComponent : public Component {
public:
    void update(float deltaTime, TransformComponent *transform);

    float mMoveSpeed;
    float mRotateSpeed;

    float mYaw = 0.0f;
    float mPitch = 0.0f;
    float mMinPitch = -89.0f;
    float mMaxPitch = 89.0f;

    bool mFirstUpdate = true;
};
