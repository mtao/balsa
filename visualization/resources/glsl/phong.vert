#version 460 core
#extension GL_GOOGLE_include_directive : require
// Blinn-Phong surfaces ("balsa.phong").
//
// Defines:
//   SHADING_FLAT / SHADING_GOURAUD / SHADING_PHONG / SHADING_UNLIT
//   HAS_NORMALS   normal stream bound
//   HAS_SCALARS   scalar stream bound (up to 4 raw components)
//   COLORMAP      colour by the scalar, through "colormap.glsl"'s
//                 vec4 colormap(float)

#include "balsa/scene.glsl"

layout(location = 0) in vec3 inPosition;
#if defined(HAS_NORMALS)
layout(location = 1) in vec3 inNormal;
#endif
#if defined(HAS_SCALARS)
layout(location = 2) in vec4 inScalar;   // missing components read as (0, 0, 0, 1)
#endif

#include "balsa/phong_material.glsl"

layout(location = 0) out vec3 v_world_pos;
#if defined(SHADING_GOURAUD)
layout(location = 1) out vec3 v_diffuse;
layout(location = 4) out vec3 v_specular;
#elif defined(SHADING_FLAT)
layout(location = 1) flat out vec3 v_normal;
#else
layout(location = 1) out vec3 v_normal;
#endif
layout(location = 2) out vec4 v_color;
layout(location = 3) out float v_scalar;

// One scalar from the raw attribute: a component, or the p-norm over the
// attribute's components (missing components must not contribute).
float reduce_scalar(vec4 v) {
    int n = int(u_scalar_select.z);
    if (u_scalar_select.x < 0.5) {
        int c = int(u_scalar_select.y);
        return c < n ? v[c] : 0.0;
    }
    float p = max(u_scalar_select.y, 1.0);
    float sum = 0.0;
    for (int i = 0; i < n; ++i) {
        sum += pow(abs(v[i]), p);
    }
    return pow(sum, 1.0 / p);
}

void main() {
    vec4 world_pos = model * vec4(inPosition, 1.0);
    v_world_pos = world_pos.xyz;
    gl_Position = projection * view * world_pos;

    vec3 N = vec3(0.0, 0.0, 1.0);
#if defined(HAS_NORMALS)
    N = normalize(mat3(normal_matrix) * inNormal);
#endif

    v_color = u_color;
#if defined(HAS_SCALARS)
    v_scalar = reduce_scalar(inScalar);
#else
    v_scalar = 0.0;
#endif

#if defined(SHADING_GOURAUD)
    // Lit per vertex; the fragment shader applies the terms to its colour
    // (colormapped colours are only known per fragment).
    phong_terms(N, normalize(camera_pos.xyz - v_world_pos), v_diffuse, v_specular);
#else
    v_normal = N;
#endif
}
