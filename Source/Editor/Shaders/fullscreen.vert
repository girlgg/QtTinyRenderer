#version 440

layout(location = 0) out vec2 vUV;

out gl_PerVertex {
    vec4 gl_Position;
};

void main() {
    vUV = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
    gl_Position = vec4(vUV * 2.0 - 1.0, 0.0, 1.0);
}
