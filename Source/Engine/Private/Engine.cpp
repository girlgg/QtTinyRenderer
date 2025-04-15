#include "Engine.h"
#include <iostream>

void Engine::run() {
    std::cout << "Engine running with message: " << Common::getSharedMessage() << std::endl;
}
