#version 460 core
#extension GL_GOOGLE_include_directive : require
// Blinn-Phong surfaces ("balsa.phong"); defines as phong.vert.

#include "balsa/scene.glsl"
#include "balsa/phong_material.glsl"
#if defined(COLORMAP)
#include "colormap.glsl"
#endif

layout(location = 0) in vec3 v_world_pos;
#if defined(SHADING_GOURAUD)
layout(location = 1) in vec3 v_diffuse;
layout(location = 4) in vec3 v_specular;
#elif defined(SHADING_FLAT)
layout(location = 1) flat in vec3 v_normal;   // unused: face normals below
#else
layout(location = 1) in vec3 v_normal;
#endif
layout(location = 2) in vec4 v_color;
layout(location = 3) in float v_scalar;

layout(location = 0) out vec4 outColor;

void main() {
    vec4 color = v_color;
#if defined(HAS_SCALARS) && defined(COLORMAP)
    float range = max(u_scalar_range.y - u_scalar_range.x, 1e-6);
    color.rgb = colormap(clamp((v_scalar - u_scalar_range.x) / range, 0.0, 1.0)).rgb;
#endif

#if defined(SHADING_UNLIT)
    outColor = color;
#elif defined(SHADING_GOURAUD)
    outColor = vec4(v_diffuse * color.rgb + v_specular, color.a);
#else
    // Interpolated normals for SHADING_PHONG when bound, else face normals.
#if defined(SHADING_PHONG) && defined(HAS_NORMALS)
    vec3 N = normalize(v_normal);
#else
    vec3 N = normalize(cross(dFdx(v_world_pos), dFdy(v_world_pos)));
#endif
    vec3 diffuse;
    vec3 specular;
    phong_terms(N, normalize(camera_pos.xyz - v_world_pos), diffuse, specular);
    outColor = vec4(diffuse * color.rgb + specular, color.a);
#endif
}
