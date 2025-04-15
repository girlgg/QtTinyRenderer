#pragma once

#include <QMatrix4x4>

#include "Component/CameraComponent.h"
#include "Component/CameraControllerComponent.h"
#include "Component/TransformComponent.h"

class Camera {
public:
    TransformComponent transform;
    CameraComponent camera;
    CameraControllerComponent controller;
};
