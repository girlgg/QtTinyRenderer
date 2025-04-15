#pragma once
#include "Component.h"

struct RenderableComponent : public Component {
    bool isVisible = true;
};
