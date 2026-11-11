#version 460 core
#extension GL_GOOGLE_include_directive : require
// Blinn-Phong surfaces ("balsa.phong"); defines as phong.vert.

#include "balsa/scene.glsl"
#include "balsa/phong_material.glsl"
#include "balsa/debug_view.glsl"
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

// Debug channels: this renderer's debug_channels(), from 1 (0 = off).
const int DEBUG_DIFFUSE = 1;
const int DEBUG_SPECULAR = 2;
const int DEBUG_SCALAR = 3;

void main() {
    vec4 color = v_color;
    float scalar = 0.0;
#if defined(HAS_SCALARS)
    float range = max(u_scalar_range.y - u_scalar_range.x, 1e-6);
    scalar = clamp((v_scalar - u_scalar_range.x) / range, 0.0, 1.0);
#if defined(COLORMAP)
    color.rgb = colormap(scalar).rgb;
#endif
#endif

    // Unlit surfaces are fully diffuse.
    vec3 diffuse = vec3(1.0);
    vec3 specular = vec3(0.0);
#if defined(SHADING_GOURAUD)
    diffuse = v_diffuse;
    specular = v_specular;
#elif !defined(SHADING_UNLIT)
    // Interpolated normals for SHADING_PHONG when bound, else face normals.
#if defined(SHADING_PHONG) && defined(HAS_NORMALS)
    vec3 N = normalize(v_normal);
#else
    vec3 N = normalize(cross(dFdx(v_world_pos), dFdy(v_world_pos)));
#endif
    phong_terms(N, normalize(camera_pos.xyz - v_world_pos), diffuse, specular);
#endif

    switch (int(u_scalar_select.w + 0.5)) {
    case DEBUG_DIFFUSE: outColor = vec4(diffuse, 1.0); return;
    case DEBUG_SPECULAR: outColor = vec4(specular, 1.0); return;
    case DEBUG_SCALAR: outColor = show_data(scalar); return;
    default: break;
    }
    outColor = vec4(diffuse * color.rgb + specular, color.a);
}
