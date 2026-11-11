#version 460 core
#extension GL_GOOGLE_include_directive : require
// Any attribute shown as an unlit colour ("balsa.attribute_color");
// defines as attribute_color.vert.

#include "balsa/scene.glsl"
#include "balsa/debug_view.glsl"

layout(location = 0) in vec3 v_world_pos;
layout(location = 1) in vec4 v_value;

layout(set = 1, binding = 0) uniform Material {
    vec4 u_min;       // per component
    vec4 u_max;       // per component
    vec4 u_options;   // x = component count, y = fractional (> 0.5)
};

layout(location = 0) out vec4 outColor;

void main() {
#if defined(HAS_ATTRIBUTE)
    vec4 value = v_value;
    int count = int(u_options.x + 0.5);
#else
    // Face normals, as a signed range.
    vec4 value = vec4(normalize(cross(dFdx(v_world_pos), dFdy(v_world_pos))), 0.0);
    int count = 3;
#endif
    vec4 t = u_options.y > 0.5 ? fract(value)
                               : (value - u_min) / max(u_max - u_min, vec4(1e-12));
    vec3 rgb = count == 1 ? vec3(t.x) : vec3(t.x, t.y, count == 2 ? 0.0 : t.z);
    outColor = show_data(rgb);
}
