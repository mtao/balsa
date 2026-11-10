#version 460 core
#extension GL_GOOGLE_include_directive : require
// Unlit lines and points ("balsa.unlit_lines", "balsa.unlit_points").

#include "balsa/scene.glsl"

layout(location = 0) in vec3 inPosition;

layout(set = 1, binding = 0) uniform Material {
    vec4 u_color;
    vec4 u_size;   // x = point size in pixels
};

void main() {
    gl_Position = projection * view * model * vec4(inPosition, 1.0);
    gl_PointSize = u_size.x;
}
