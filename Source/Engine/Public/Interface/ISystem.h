#pragma once

class World;

class ISystem {
public:
    virtual ~ISystem() = default;
    virtual void update(World* world, float deltaTime) = 0;
};
