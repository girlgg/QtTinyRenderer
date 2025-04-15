#version 450 core
// Or #version 310 es etc. depending on qsb compilation target

// Input Vertex Attributes (matches VertexData and inputLayout)
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

// Set 2: Instance Data (Bound per-instance via dynamic UBO offset)
layout (binding = 3, std140) uniform InstanceUbo {
    mat4 model; // Model matrix for the current instance
    // Ensure layout matches C++ InstanceUniformBlock including any padding
} instanceData;


// --- Outputs (to Fragment Shader) ---
layout (location = 0) out vec3 fragPos;      // World-space position
layout (location = 1) out vec3 fragNormal;   // World-space normal
layout (location = 2) out vec2 fragTexCoord; // Pass-through texture coordinate

void main() {
    // Calculate world-space position
    vec4 worldPos = instanceData.model * vec4(inPosition, 1.0);
    fragPos = worldPos.xyz;

    // Calculate world-space normal (Simple version)
    // For non-uniform scaling, use transpose(inverse(mat3(instanceData.model)))
    mat3 normalMatrix = mat3(instanceData.model);
    fragNormal = normalize(normalMatrix * inNormal);

    // Pass texture coordinates through
    fragTexCoord = inTexCoord;

    // Calculate final clip-space position
    gl_Position = cameraData.projection * cameraData.view * worldPos;
}
