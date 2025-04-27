#version 450 core

// --- Inputs (from Vertex Shader) ---
layout (location = 0) in vec3 fragPosWorld;
layout (location = 1) in vec2 fragTexCoord;
layout (location = 2) in mat3 TBN;
layout (location = 5) in vec3 viewPosWorld;

// --- Uniforms ---

layout(binding = 0, std140) uniform CameraUbo {
    mat4 view;
    mat4 projection;
    vec3 viewPos;
} cameraData;

// 更新灯光UBO
#define MAX_POINT_LIGHTS 4// 与 C++ 中保持一致
struct PointLight {
    vec3 position;
    vec3 color;
    float constant;
    float linear;
    float quadratic;
    float padding1;
};
struct DirLight {
    vec3 direction;
    vec3 color;
    int enabled;
    float padding1;
    float padding2;
    float padding3;
};

layout(binding = 1, std140) uniform LightingUbo {
    DirLight dirLight;
    PointLight pointLights[MAX_POINT_LIGHTS];
    int numPointLights;
    float padding1;
    float padding2;
    float padding3;
} lightData;

// Material Texture Samplers
layout (binding = 2) uniform sampler2D albedoMap;
layout (binding = 4) uniform sampler2D normalMap;
layout (binding = 5) uniform sampler2D metallicRoughnessMap;
layout (binding = 6) uniform sampler2D aoMap;
layout (binding = 7) uniform sampler2D emissiveMap;

// --- Output ---

layout(location = 0) out vec4 outColor;

const float PI = 3.14159265359;

// PBR 函数 (Cook-Torrance BRDF with GGX NDF and Schlick-GGX Geometry)

// 正态分布 (GGX Trowbridge-Reitz)
float DistributionGGX(vec3 N, vec3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;

    float nom   = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return nom / max(denom, 0.0000001);
}

// 几何 (Schlick-GGX for Smith's Method)
float GeometrySchlickGGX(float NdotV, float roughness) {
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0; // Schlick-GGX

    float nom   = NdotV;
    float denom = NdotV * (1.0 - k) + k;

    return nom / max(denom, 0.0000001); // Prevent divide by zero
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);

    return ggx1 * ggx2;
}

// 菲涅尔 (Schlick Approximation)
vec3 fresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
    // return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

// 采样法线贴图
vec3 getNormalFromMap() {
    vec3 tangentNormal = texture(normalMap, fragTexCoord).xyz * 2.0 - 1.0;
    if (length(tangentNormal) < 0.1) {
        tangentNormal = vec3(0.0, 0.0, 1.0);
    }

    vec3 N = normalize(TBN * normalize(tangentNormal));
    return N;
}

void main() {
    // 基本材质属性
    vec4 albedoSample = texture(albedoMap, fragTexCoord);
    vec3 albedo = albedoSample.rgb; // pow(albedoSample.rgb, vec3(2.2)); // 伽马矫正？

    vec4 metallicRoughnessSample = texture(metallicRoughnessMap, fragTexCoord);
    float metallic = metallicRoughnessSample.b;
    float roughness = metallicRoughnessSample.g;
    float ao = texture(aoMap, fragTexCoord).r;

    vec3 emissive = texture(emissiveMap, fragTexCoord).rgb; // pow(texture(emissiveMap, fragTexCoord).rgb, vec3(2.2)); // 伽马矫正？

    // 法线和视线
    vec3 N = getNormalFromMap(); // 世界空间法线
    vec3 V = normalize(viewPosWorld - fragPosWorld); // 世界空间视图方向

    // 反射 (F0)
    vec3 F0 = vec3(0.04);
    F0 = mix(F0, albedo, metallic);

    vec3 Lo = vec3(0.0);

    // 定向光
    if (lightData.dirLight.enabled > 0) {
        vec3 L = normalize(-lightData.dirLight.direction);
        vec3 H = normalize(V + L); // 半程向量
        vec3 radiance = lightData.dirLight.color; // Irradiance

        // Cook-Torrance BRDF
        float NDF = DistributionGGX(N, H, roughness);
        float G   = GeometrySmith(N, V, L, roughness);
        vec3  F   = fresnelSchlick(max(dot(H, V), 0.0), F0);

        vec3 kS = F; // 高光菲涅尔
        vec3 kD = vec3(1.0) - kS; // Diffuse (能量守恒)
        kD *= (1.0 - metallic);

        float NdotL = max(dot(N, L), 0.0);

        // Specular BRDF
        vec3 numerator = NDF * G * F;
        float denominator = 4.0 * max(dot(N, V), 0.0) * NdotL + 0.0001; // 防止除以 0
        vec3 specular = numerator / denominator;

        Lo += (kD * albedo / PI + specular) * radiance * NdotL;
    }

    // 点光源
    for(int i = 0; i < lightData.numPointLights; ++i) {
        // 世界空间光照方向
        vec3 L = normalize(lightData.pointLights[i].position - fragPosWorld);
        vec3 H = normalize(V + L); // 半程向量
        float distance = length(lightData.pointLights[i].position - fragPosWorld);

        // 衰减
        float attenuation = 1.0 / (lightData.pointLights[i].constant +
        lightData.pointLights[i].linear * distance +
        lightData.pointLights[i].quadratic * (distance * distance));

        vec3 radiance = lightData.pointLights[i].color * attenuation;

        // Cook-Torrance BRDF
        float NDF = DistributionGGX(N, H, roughness);
        float G   = GeometrySmith(N, V, L, roughness);
        vec3  F   = fresnelSchlick(max(dot(H, V), 0.0), F0);

        vec3 kS = F;
        vec3 kD = vec3(1.0) - kS;
        kD *= (1.0 - metallic);

        float NdotL = max(dot(N, L), 0.0);

        vec3 numerator = NDF * G * F;
        float denominator = 4.0 * max(dot(N, V), 0.0) * NdotL + 0.0001;
        vec3 specular = numerator / denominator;

        Lo += (kD * albedo / PI + specular) * radiance * NdotL;
    }

    // 环境光遮蔽
    vec3 ambient = vec3(0.03 * albedo * ao);
    vec3 color = ambient + Lo;

    color += emissive;

    // HDR -> LDR
    // color = color / (color + vec3(1.0)); // 色调映射
    // color = pow(color, vec3(1.0/2.2)); // 伽马矫正

    outColor = vec4(color, albedoSample.a);
    // outColor = vec4(N * 0.5 + 0.5, 1.0); // Debug: Visualize Normals
    // outColor = vec4(metallic, metallic, metallic, 1.0); // Debug: Visualize Metallic
    // outColor = vec4(roughness, roughness, roughness, 1.0); // Debug: Visualize Roughness
    // outColor = vec4(ao, ao, ao, 1.0); // Debug: Visualize AO
}
