#pragma once

#include "Interface/ISystem.h"

class World;

class CameraSystem : public ISystem {
    void update(World *world, float deltaTime) override;
};

