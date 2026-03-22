#version 460 core

// ──────────────────────────────────────────────────────────────────────
// Mesh uber-vertex-shader
//
// Preprocessor defines (set by MeshShader):
//   HAS_NORMALS          — vertex normal attribute is present
//   SHADING_FLAT         — use flat interpolation on normal
//   SHADING_PHONG        — smooth interpolation (default if neither FLAT)
//   SHADING_GOURAUD      — treated as PHONG for now
//   COLOR_UNIFORM        — single uniform color
//   COLOR_PER_VERTEX     — per-vertex RGB attribute
//   COLOR_SCALAR_FIELD   — per-vertex scalar (colormap in fragment shader)
// ──────────────────────────────────────────────────────────────────────

// --- Vertex inputs (separate bindings) ---
layout(location = 0) in vec3 inPosition;

#if defined(HAS_NORMALS)
layout(location = 1) in vec3 inNormal;
#endif

#if defined(COLOR_PER_VERTEX)
layout(location = 2) in vec3 inColor;
#endif

#if defined(COLOR_SCALAR_FIELD)
layout(location = 3) in float inScalar;
#endif

// --- Uniform buffer objects ---
layout(binding = 0) uniform Transforms {
    mat4 model;
    mat4 view;
    mat4 projection;
    mat4 normal_matrix;   // transpose(inverse(model)), stored as mat4 for alignment
};

layout(binding = 1) uniform Material {
    vec4 u_uniform_color;       // rgba
    vec4 u_light_dir;           // xyz = direction, w = ambient_strength
    vec4 u_specular_params;     // xyz = specular_color, w = shininess
    vec4 u_scalar_params;       // x = scalar_min, y = scalar_max, z = point_size, w = two_sided
    vec4 u_wireframe_color;     // rgba (used when RENDER_WIREFRAME is defined)
};

// --- Outputs to fragment shader ---
layout(location = 0) out vec3 v_world_pos;

#if defined(SHADING_FLAT)
layout(location = 1) flat out vec3 v_normal;
#else
layout(location = 1) out vec3 v_normal;
#endif

layout(location = 2) out vec3 v_color;

#if defined(COLOR_SCALAR_FIELD)
layout(location = 3) out float v_scalar;
#endif


void main() {
    vec4 world_pos = model * vec4(inPosition, 1.0);
    v_world_pos = world_pos.xyz;
    gl_Position = projection * view * world_pos;

    // Normal
#if defined(HAS_NORMALS)
    v_normal = normalize(mat3(normal_matrix) * inNormal);
#else
    v_normal = vec3(0.0, 0.0, 1.0);
#endif

    // Base colour / scalar pass-through
#if defined(COLOR_PER_VERTEX)
    v_color = inColor;
#elif defined(COLOR_SCALAR_FIELD)
    v_scalar = inScalar;
    v_color = vec3(1.0);   // placeholder; colormap applied in fragment
#else
    // COLOR_UNIFORM (default)
    v_color = u_uniform_color.rgb;
#endif

    // Point size (for point layer pass)
    gl_PointSize = u_scalar_params.z;
}
