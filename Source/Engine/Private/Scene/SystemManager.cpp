#include "Scene/SystemManager.h"

void SystemManager::updateAll(World *world, float deltaTime) {
    for (auto& system : mSystems) {
        system->update(world, deltaTime);
    }
}
