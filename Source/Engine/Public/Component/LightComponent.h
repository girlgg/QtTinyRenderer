#pragma once

#include <QVector3D>

#include "Component.h"

enum class LightType { Directional, Point, Spot };

struct LightComponent : public Component {
    LightType type = LightType::Directional;
    QVector3D color = {1.0f, 1.0f, 1.0f};
    float intensity = 1.0f;
    // --- 方向光 ---
    // 使用 Transform 组件设置方向
    // --- 点/聚光 ---
    float constantAttenuation = 1.0f;
    float linearAttenuation = 0.09f;
    float quadraticAttenuation = 0.032f;
    // --- 聚光 ---
    float cutOffAngle = 12.5f;
    float outerCutOffAngle = 15.0f;
    // --- 通用光线 ---
    QVector3D ambient = {0.1f, 0.1f, 0.1f};
    QVector3D diffuse = {0.8f, 0.8f, 0.8f};
    QVector3D specular = {0.5f, 0.5f, 0.5f};
};
