#pragma once

#include <QVector>

#include "Interface/ISystem.h"
#include "Resources/ShaderBundle.h"

class World;

class SystemManager {
public:
    template<typename T, typename... Args>
    T *addSystem(Args &&... args);

    void updateAll(World *world, float deltaTime);

private:
    QVector<QSharedPointer<ISystem> > mSystems;
};

template<typename T, typename... Args>
T *SystemManager::addSystem(Args &&... args) {
    static_assert(std::is_base_of<ISystem, T>::value, "T must inherit from ISystem");
    auto system = QSharedPointer<T>::create(std::forward<Args>(args)...);
    T *ptr = system.get();
    mSystems.emplace_back(std::move(system));
    return ptr;
}
