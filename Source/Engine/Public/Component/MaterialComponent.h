#pragma once

#include <QVector3D>
#include <QString>
#include "Component.h"

struct MaterialComponent : Component {
    QVector3D albedoColor = {1.0f, 1.0f, 1.0f};
    QString textureResourceId = ":/img/Images/container.png";
    // QString diffuseTextureResourceId = ":/img/Images/container.png";
    // QString specularTextureResourceId = ":/img/Images/container2_specular.png";
    float shininess = 32.0f;
    float roughness = 0.5f;
    float metallic = 0.1f;
    bool rhiDataDirty = true;
};
