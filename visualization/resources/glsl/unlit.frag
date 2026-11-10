#version 460 core
// Unlit lines and points ("balsa.unlit_lines", "balsa.unlit_points").

layout(set = 1, binding = 0) uniform Material {
    vec4 u_color;
    vec4 u_size;
};

layout(location = 0) out vec4 outColor;

void main() {
    outColor = u_color;
}
