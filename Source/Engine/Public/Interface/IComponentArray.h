#pragma once

class IComponentArray{
public:
    virtual ~IComponentArray() = default;
    virtual void removeEntity(EntityID entity) = 0;
};
