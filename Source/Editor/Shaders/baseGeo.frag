#version 450 core
// Or #version 310 es etc.

// --- Inputs (from Vertex Shader) ---
layout(location = 0) in vec3 fragPos;// World-space position
layout(location = 1) in vec3 fragNormal;// World-space normal (needs normalization)
layout(location = 2) in vec2 fragTexCoord;// Texture coordinate

// --- Uniforms ---

// Set 0: Global Data
layout(binding = 0, std140) uniform CameraUbo {
    mat4 view;// Usually unused in fragment shader
    mat4 projection;// Usually unused in fragment shader
    vec3 viewPos;// Camera position in world space
} cameraData;

layout(binding = 1, std140) uniform LightingUbo {
    vec3 dirLightDirection;// Direction FROM the light TO the origin
    vec3 dirLightAmbient;
    vec3 dirLightDiffuse;
    vec3 dirLightSpecular;
// Add point/spot light arrays here if needed
} lightData;

// Set 1: Material Data (Texture/Sampler)
layout(binding = 2) uniform sampler2D texSampler;// Texture

// Set 2: Instance Data (Not needed in fragment shader for this example)

// --- Output ---
layout(location = 0) out vec4 outColor;

void main() {
    // Normalize interpolated inputs
    vec3 norm = normalize(fragNormal);
    vec3 viewDir = normalize(cameraData.viewPos - fragPos);// Direction from fragment to camera

    // Sample texture
    vec4 textureColor = texture(texSampler, fragTexCoord);

    // Optional: Basic Alpha Testing
    // if(textureColor.a < 0.1) discard;

    // --- Lighting Calculation (Directional Light - Blinn-Phong Example) ---
    vec3 ambient = lightData.dirLightAmbient * textureColor.rgb;

    vec3 lightDir = normalize(-lightData.dirLightDirection);// Direction from fragment TO light
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * lightData.dirLightDiffuse * textureColor.rgb;

    vec3 halfwayDir = normalize(lightDir + viewDir);
    float specPower = 32.0;// Shininess factor (could be from material)
    float spec = pow(max(dot(norm, halfwayDir), 0.0), specPower);
    vec3 specular = spec * lightData.dirLightSpecular;// Specular usually white or light color

    vec3 result = ambient + diffuse + specular;

    outColor = vec4(result, textureColor.a);
}
