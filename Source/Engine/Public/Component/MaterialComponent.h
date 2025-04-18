#pragma once

#include <QVector3D>
#include <QString>
#include "Component.h"
#include "CommonRender.h"

struct MaterialComponent : Component {
    // float shininess = 32.0f;
    // float roughness = 0.5f;
    // float metallic = 0.1f;
    // bool rhiDataDirty = true;

    // PBR Parameters
    QVector3D albedoFactor = {1.0f, 1.0f, 1.0f};
    float metallicFactor = 0.1f;
    float roughnessFactor = 0.8f;
    QVector3D emissiveFactor = {0.0f, 0.0f, 0.0f};
    float aoStrength = 1.0f;
    // PBR Texture Map Resource IDs
    QString albedoMapResourceId = DEFAULT_WHITE_TEXTURE_ID;
    QString specularMapResourceId = DEFAULT_WHITE_TEXTURE_ID;
    QString normalMapResourceId = DEFAULT_NORMAL_MAP_ID;
    QString metallicRoughnessMapResourceId = DEFAULT_METALROUGH_TEXTURE_ID;
    QString ambientOcclusionMapResourceId = DEFAULT_WHITE_TEXTURE_ID;
    QString emissiveMapResourceId = DEFAULT_BLACK_TEXTURE_ID;
};
