#version 460 core
#extension GL_GOOGLE_include_directive : require
// glTF metallic-roughness surfaces ("balsa.pbr"); see pbr.frag.
//
// Defines, one per bound optional stream:
//   HAS_NORMALS, HAS_TANGENTS, HAS_TEXTURE_COORDINATES_0,
//   HAS_TEXTURE_COORDINATES_1, HAS_COLORS

layout(location = 0) in vec3 inPosition;
#if defined(HAS_NORMALS)
layout(location = 1) in vec3 inNormal;
#endif
#if defined(HAS_TANGENTS)
layout(location = 2) in vec4 inTangent;   // xyz, w = bitangent sign
#endif
#if defined(HAS_TEXTURE_COORDINATES_0)
layout(location = 3) in vec2 inTextureCoordinates0;
#endif
#if defined(HAS_TEXTURE_COORDINATES_1)
layout(location = 4) in vec2 inTextureCoordinates1;
#endif
#if defined(HAS_COLORS)
layout(location = 5) in vec4 inColor;     // rgb reads alpha as 1
#endif

#include "balsa/scene.glsl"

layout(location = 0) out vec3 v_world_pos;
layout(location = 1) out vec3 v_normal;
layout(location = 2) out vec4 v_tangent;
layout(location = 3) out vec2 v_texture_coordinates0;
layout(location = 4) out vec2 v_texture_coordinates1;
layout(location = 5) out vec4 v_color;

void main() {
    vec4 world_pos = model * vec4(inPosition, 1.0);
    v_world_pos = world_pos.xyz;
    gl_Position = projection * view * world_pos;

#if defined(HAS_NORMALS)
    v_normal = normalize(mat3(normal_matrix) * inNormal);
#else
    v_normal = vec3(0.0);
#endif
#if defined(HAS_TANGENTS)
    v_tangent = vec4(normalize(mat3(model) * inTangent.xyz), inTangent.w);
#else
    v_tangent = vec4(0.0);
#endif
#if defined(HAS_TEXTURE_COORDINATES_0)
    v_texture_coordinates0 = inTextureCoordinates0;
#else
    v_texture_coordinates0 = vec2(0.0);
#endif
#if defined(HAS_TEXTURE_COORDINATES_1)
    v_texture_coordinates1 = inTextureCoordinates1;
#else
    v_texture_coordinates1 = vec2(0.0);
#endif
#if defined(HAS_COLORS)
    v_color = inColor;
#else
    v_color = vec4(1.0);
#endif
}
