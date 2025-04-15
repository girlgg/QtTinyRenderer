#pragma once

#include <QVector3D>

#include "Component.h"

enum class LightType { Directional, Point, Spot };

struct LightComponent : public Component {
    LightType type = LightType::Directional;
    QVector3D color = {1.0f, 1.0f, 1.0f};
    float intensity = 1.0f;
    // Directional
    QVector3D direction = {0.0f, -1.0f, 0.0f}; // Needs TransformComponent's rotation too
    // Point/Spot
    float constantAttenuation = 1.0f;
    float linearAttenuation = 0.09f;
    float quadraticAttenuation = 0.032f;
    // Spot
    float cutOffAngle = 12.5f; // Inner cone angle (degrees)
    float outerCutOffAngle = 15.0f; // Outer cone angle (degrees)

    // Common lighting model terms (can be derived)
    QVector3D ambient = {0.1f, 0.1f, 0.1f};
    QVector3D diffuse = {0.8f, 0.8f, 0.8f}; // Often color * intensity
    QVector3D specular = {0.5f, 0.5f, 0.5f};
};
