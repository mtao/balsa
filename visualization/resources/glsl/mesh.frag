#version 460 core

// ──────────────────────────────────────────────────────────────────────
// Mesh uber-fragment-shader
//
// Preprocessor defines (set by MeshShader):
//   SHADING_FLAT / SHADING_PHONG / SHADING_GOURAUD
//   COLOR_UNIFORM / COLOR_PER_VERTEX / COLOR_SCALAR_FIELD
//   RENDER_WIREFRAME — bypass lighting, output wireframe_color directly
//
// When COLOR_SCALAR_FIELD is defined, the colormap function
//   vec4 colormap(float x)
// is prepended to this source by MeshShader before compilation.
// ──────────────────────────────────────────────────────────────────────

// --- Inputs from vertex shader ---
layout(location = 0) in vec3 v_world_pos;

#if defined(SHADING_FLAT)
layout(location = 1) flat in vec3 v_normal;
#else
layout(location = 1) in vec3 v_normal;
#endif

layout(location = 2) in vec3 v_color;

#if defined(COLOR_SCALAR_FIELD)
layout(location = 3) in float v_scalar;
#endif

// --- UBO ---
layout(binding = 1) uniform Material {
    vec4 u_uniform_color;       // rgba
    vec4 u_light_dir;           // xyz = direction, w = ambient_strength
    vec4 u_specular_params;     // xyz = specular_color, w = shininess
    vec4 u_scalar_params;       // x = scalar_min, y = scalar_max, z = point_size, w = two_sided
    vec4 u_wireframe_color;     // rgba (used when RENDER_WIREFRAME is defined)
};

// --- Output ---
layout(location = 0) out vec4 outColor;


void main() {
#if defined(RENDER_WIREFRAME)
    // Wireframe mode: use wireframe color directly, no lighting
    outColor = u_wireframe_color;
#else
    // ── 1. Determine base colour ──
    vec3 base_color;

#if defined(COLOR_SCALAR_FIELD)
    float range = max(u_scalar_params.y - u_scalar_params.x, 1e-6);
    float t = clamp((v_scalar - u_scalar_params.x) / range, 0.0, 1.0);
    base_color = colormap(t).rgb;
#else
    base_color = v_color;
#endif

    // ── 2. Lighting ──
#if defined(SHADING_FLAT) || defined(SHADING_PHONG) || defined(SHADING_GOURAUD)
    vec3 N = normalize(v_normal);

    // Two-sided lighting: flip normal for back-faces
    bool two_sided = (u_scalar_params.w > 0.5);
    if (two_sided && !gl_FrontFacing) {
        N = -N;
    }

    vec3 L = normalize(u_light_dir.xyz);
    float ambient = u_light_dir.w;
    float diff = max(dot(N, L), 0.0);

    // Blinn-Phong specular
    vec3 V = normalize(-v_world_pos);
    vec3 H = normalize(L + V);
    float shininess = u_specular_params.w;
    float spec = pow(max(dot(N, H), 0.0), shininess);

    vec3 result = ambient * base_color
                + diff * base_color
                + spec * u_specular_params.xyz;

    outColor = vec4(result, u_uniform_color.a);
#else
    // No shading — unlit output
    outColor = vec4(base_color, u_uniform_color.a);
#endif

#endif // RENDER_WIREFRAME
}
