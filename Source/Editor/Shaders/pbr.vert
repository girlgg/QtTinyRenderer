#version 450 core

layout (location = 0) in vec3 inPosition;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec2 inTexCoord;
layout (location = 3) in vec3 inTangent;

// --- Uniforms ---

layout (binding = 0, std140) uniform CameraUbo {
    mat4 view;
    mat4 projection;
    vec3 viewPos;
} cameraData;

const int MAX_INSTANCES = 1024;

struct InstanceUniformBlock {
    mat4 model;
};

layout (binding = 3, std140) uniform InstanceBuffer {
    InstanceUniformBlock instanceData[MAX_INSTANCES];
} instanceBuffer;

// --- 输出到片元着色器 ---
layout (location = 0) out vec3 fragPosWorld;    // 世界空间位置
layout (location = 1) out vec2 fragTexCoord;    // 纹理坐标
layout (location = 2) out mat3 TBN;             // TBN 矩阵 (切线空间 -> 世界空间) mat3，占3个location
layout (location = 5) out vec3 viewPosWorld;    // 观察者位置（世界空间）

void main() {
    mat4 currentModelMatrix = instanceBuffer.instanceData[gl_InstanceIndex].model;
    vec4 worldPos = currentModelMatrix * vec4(inPosition, 1.0);

    fragPosWorld = worldPos.xyz;
    fragTexCoord = inTexCoord;
    viewPosWorld = cameraData.viewPos;

    // 计算 TBN 矩阵
    // 法线矩阵 (用于变换法线和切线)
    mat3 normalMatrix = mat3(transpose(inverse(currentModelMatrix)));

    vec3 T = normalize(normalMatrix * inTangent);
    vec3 N = normalize(normalMatrix * inNormal);
    // 重新正交化切线，使其与法线垂直
    T = normalize(T - dot(T, N) * N);
    // 计算副切线 (Bitangent)
    // 左右手坐标系，左手坐标系 B = cross(T, N)
    vec3 B = cross(N, T);
    // 构建从切线空间到世界空间的变换矩阵
    TBN = mat3(T, B, N);

    gl_Position = cameraData.projection * cameraData.view * worldPos;
}
