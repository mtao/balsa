#version 460 core
#extension GL_GOOGLE_include_directive : require
// Any attribute shown as an unlit colour ("balsa.attribute_color").
//
// Defines:
//   HAS_ATTRIBUTE  the visualized stream is bound (else face normals)

#include "balsa/scene.glsl"

layout(location = 0) in vec3 inPosition;
#if defined(HAS_ATTRIBUTE)
layout(location = 1) in vec4 inValue;   // missing components read as (0, 0, 0, 1)
#endif

layout(location = 0) out vec3 v_world_pos;
layout(location = 1) out vec4 v_value;

void main() {
    vec4 world_pos = model * vec4(inPosition, 1.0);
    v_world_pos = world_pos.xyz;
    gl_Position = projection * view * world_pos;
#if defined(HAS_ATTRIBUTE)
    v_value = inValue;
#else
    v_value = vec4(0.0);
#endif
}
