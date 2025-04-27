#pragma once

#include <QVector3D>
#include <QMatrix4x4>
#include <QGenericMatrix>
#include <QVector>

// --- Constants ---
const QString BUILTIN_CUBE_MESH_ID = "builtin://meshes/cube";
const QString BUILTIN_SPHERE_MESH_ID = "builtin://meshes/sphere";
const QString BUILTIN_PYRAMID_MESH_ID = "builtin://meshes/pyramid";

const QString DEFAULT_WHITE_TEXTURE_ID = "builtin://textures/default_white"; // 1x1 white
const QString DEFAULT_NORMAL_MAP_ID = "builtin://textures/default_normal"; // 1x1 flat normal (0.5, 0.5, 1.0)
const QString DEFAULT_BLACK_TEXTURE_ID = "builtin://textures/default_black"; // 1x1 black
const QString DEFAULT_METALROUGH_TEXTURE_ID = "builtin://textures/default_metalrough"; // 1x1 default metal/rough


// --- Shader Binding Points ---
const quint32 BINDING_CAMERA_UBO = 0;
const quint32 BINDING_LIGHTING_UBO = 1;
const quint32 BINDING_ALBEDO_MAP = 2;
const quint32 BINDING_INSTANCE_UBO = 3;
const quint32 BINDING_NORMAL_MAP = 4;
const quint32 BINDING_METALROUGH_MAP = 5;
const quint32 BINDING_AO_MAP = 6;
const quint32 BINDING_EMISSIVE_MAP = 7;

// --- UBO Structures ---

struct alignas(16) CameraUniformBlock {
    QGenericMatrix<4, 4, float> view;
    QGenericMatrix<4, 4, float> projection;
    QVector3D viewPos;
    float padding;
};

#define MAX_POINT_LIGHTS 4
#define MAX_SPOT_LIGHTS 2

struct PointLightData {
    alignas(16) QVector3D position;
    alignas(16) QVector3D color;
    alignas(4) float constant;
    alignas(4) float linear;
    alignas(4) float quadratic;
    alignas(4) float padding;
};

struct DirectionalLightData {
    alignas(16) QVector3D direction;
    alignas(16) QVector3D color;
    alignas(4) int enabled = 0;
    alignas(4) float padding1;
    alignas(4) float padding2;
    alignas(4) float padding3;
};

struct SpotLightData {
    alignas(16) QVector3D position;
    alignas(16) QVector3D direction;
    alignas(16) QVector3D ambient;
    alignas(16) QVector3D diffuse;
    alignas(16) QVector3D specular;

    alignas(4) float cutOffAngle;
    alignas(4) float outerCutOffAngle;
    alignas(4) float constantAttenuation = 1.0f;
    alignas(4) float linearAttenuation = 0.09f;
    alignas(4) float quadraticAttenuation = 0.032f;
    alignas(4) float intensity = 1.0f;
};

struct LightingUniformBlock {
    alignas(16) DirectionalLightData dirLight;
    alignas(16) PointLightData pointLights[MAX_POINT_LIGHTS];
    alignas(4) int numPointLights = 0;
    alignas(16) SpotLightData spotLights[MAX_SPOT_LIGHTS];
    alignas(4) int numSpotLights = 0;
};

struct InstanceUniformBlock {
    QGenericMatrix<4, 4, float> model;
};

// --- Vertex Data ---

struct VertexData {
    QVector3D position;
    QVector3D normal;
    QVector2D texCoord;
    QVector3D tangent;
};

const QVector<VertexData> DEFAULT_CUBE_VERTICES = {
    // Front face (+Z) - Tangent: +X (1,0,0)
    {{-0.5f, -0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}},
    {{0.5f, -0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f}, {1.0f, 0.0f, 0.0f}},
    {{0.5f, 0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}, {1.0f, 0.0f, 0.0f}},
    {{-0.5f, 0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}, {1.0f, 0.0f, 0.0f}},

    // Back face (-Z) - Tangent: -X (-1,0,0)
    {{0.5f, -0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}, {1.0f, 0.0f}, {-1.0f, 0.0f, 0.0f}},
    {{-0.5f, -0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}, {0.0f, 0.0f}, {-1.0f, 0.0f, 0.0f}},
    {{-0.5f, 0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}, {0.0f, 1.0f}, {-1.0f, 0.0f, 0.0f}},
    {{0.5f, 0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}, {1.0f, 1.0f}, {-1.0f, 0.0f, 0.0f}},

    // Left face (-X) - Tangent: -Z (0,0,-1)
    {{-0.5f, -0.5f, -0.5f}, {-1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f, -1.0f}},
    {{-0.5f, -0.5f, 0.5f}, {-1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}, {0.0f, 0.0f, -1.0f}},
    {{-0.5f, 0.5f, 0.5f}, {-1.0f, 0.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 0.0f, -1.0f}},
    {{-0.5f, 0.5f, -0.5f}, {-1.0f, 0.0f, 0.0f}, {0.0f, 1.0f}, {0.0f, 0.0f, -1.0f}},

    // Right face (+X) - Tangent: +Z (0,0,1)
    {{0.5f, -0.5f, 0.5f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}},
    {{0.5f, -0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}},
    {{0.5f, 0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 0.0f, 1.0f}},
    {{0.5f, 0.5f, 0.5f}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f}, {0.0f, 0.0f, 1.0f}},

    // Top face (+Y) - Tangent: +X (1,0,0)
    {{-0.5f, 0.5f, 0.5f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}},
    {{0.5f, 0.5f, 0.5f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 0.0f, 0.0f}},
    {{0.5f, 0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f}, {1.0f, 0.0f, 0.0f}},
    {{-0.5f, 0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f}, {1.0f, 0.0f, 0.0f}},

    // Bottom face (-Y) - Tangent: +X (1,0,0)
    {{-0.5f, -0.5f, -0.5f}, {0.0f, -1.0f, 0.0f}, {0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}},
    {{0.5f, -0.5f, -0.5f}, {0.0f, -1.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 0.0f, 0.0f}},
    {{0.5f, -0.5f, 0.5f}, {0.0f, -1.0f, 0.0f}, {1.0f, 1.0f}, {1.0f, 0.0f, 0.0f}},
    {{-0.5f, -0.5f, 0.5f}, {0.0f, -1.0f, 0.0f}, {0.0f, 1.0f}, {1.0f, 0.0f, 0.0f}},
};
const QVector<quint16> DEFAULT_CUBE_INDICES = {
    0, 1, 2, 2, 3, 0, // Front
    4, 5, 6, 6, 7, 4, // Back
    8, 9, 10, 10, 11, 8, // Left
    12, 13, 14, 14, 15, 12, // Right
    16, 17, 18, 18, 19, 16, // Top
    20, 21, 22, 22, 23, 20 // Bottom
};

const QVector<VertexData> DEFAULT_PYRAMID_VERTICES = {
    {{0.0f, 1.0f, 0.0f}, {0.0f, 0.8f, 0.6f}, {0.5f, 1.0f}},
    {{0.0f, 0.0f, 1.0f}, {0.0f, -0.4f, 0.8f}, {0.0f, 0.0f}},
    {{0.9f, 0.0f, -0.5f}, {0.8f, -0.4f, -0.4f}, {1.0f, 0.0f}},
    {{-0.9f, 0.0f, -0.5f}, {-0.8f, -0.4f, -0.4f}, {0.5f, 0.0f}}
};

const QVector<quint16> DEFAULT_PYRAMID_INDICES = {
    0, 1, 2,
    0, 2, 3,
    0, 3, 1,
    1, 3, 2
};

constexpr uchar WHITE_PIXEL[] = {255, 255, 255, 255};
constexpr uchar BLACK_PIXEL[] = {0, 0, 0, 255};
constexpr uchar NORMAL_PIXEL[] = {128, 128, 255, 255};
constexpr uchar METALROUGH_DEFAULT_PIXEL[] = {0, 128, 0, 255};
