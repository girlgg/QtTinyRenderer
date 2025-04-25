#version 440

layout(location = 0) in vec2 vUV;
layout(location = 0) out vec4 fragColor;

layout(binding = 0) uniform sampler2D uTexture;

void main() {
    fragColor = texture(uTexture, vUV);
}
