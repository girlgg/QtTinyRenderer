#version 450 core

layout (location = 0) in vec3 inPosition;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec2 inTexCoord;

// --- Uniforms ---

// Set 0: Global Data
layout (binding = 0, std140) uniform CameraUbo {
    mat4 view;
    mat4 projection;
    vec3 viewPos; // Camera position in world space
} cameraData;

// Set 1: Material Data (Sampler/Texture) - Not needed in Vertex Shader

const int MAX_INSTANCES = 1024; // 确保和 BasePass.h/cpp 的 mMaxInstances 完全匹配

// Set 2: Instance Data
struct InstanceUniformBlock {
    mat4 model;
};

layout (binding = 3, std140) uniform InstanceBuffer {
    InstanceUniformBlock instanceData[MAX_INSTANCES];
} instanceBuffer;

// --- 输出到片元着色器 ---
layout (location = 0) out vec3 fragPos;      // World-space position
layout (location = 1) out vec3 fragNormal;   // World-space normal
layout (location = 2) out vec2 fragTexCoord; // Pass-through texture coordinate

void main() {
    mat4 currentModelMatrix = instanceBuffer.instanceData[gl_InstanceIndex].model;

    vec4 worldPos = currentModelMatrix * vec4(inPosition, 1.0);
    fragPos = worldPos.xyz;

    mat3 normalMatrix = mat3(transpose(inverse(currentModelMatrix)));
    fragNormal = normalize(normalMatrix * inNormal);

    fragTexCoord = inTexCoord;

    gl_Position = cameraData.projection * cameraData.view * worldPos;
}
