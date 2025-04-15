#pragma once

#include <QVector3D>
#include <QString>
#include "Component.h"

struct MaterialComponent : Component {
    QVector3D albedoColor = {1.0f, 1.0f, 1.0f};
    QString textureResourceId = ":/img/Images/container.png"; // Reference to texture
    float roughness = 0.5f;
    float metallic = 0.1f;
    // ... other PBR or material properties
    bool rhiDataDirty = true; // Flag for RenderingSystem
};
