#version 460 core

// ──────────────────────────────────────────────────────────────────────
// Mesh uber-vertex-shader
//
// Preprocessor defines (set by MeshShader):
//   HAS_NORMALS          — vertex normal attribute is present
//   SHADING_FLAT         — flat shading (normals computed in frag via dFdx/dFdy)
//   SHADING_PHONG        — smooth per-fragment lighting using interpolated normals
//   SHADING_GOURAUD      — lighting computed per-vertex, lit color interpolated
//   COLOR_UNIFORM        — single uniform color
//   COLOR_PER_VERTEX     — per-vertex RGBA attribute
//   COLOR_SCALAR_FIELD   — per-vertex scalar (colormap in fragment shader)
//
// Normal handling:
//   When HAS_NORMALS is defined, per-vertex normals are transformed
//   by the normal matrix and passed to the fragment shader.
//
//   For SHADING_FLAT, the fragment shader always recomputes the face
//   normal from dFdx/dFdy of the world position, so the vertex normal
//   is unused (but still output to satisfy the interface).
//
//   For SHADING_GOURAUD without HAS_NORMALS, a default up-vector
//   normal is used for per-vertex lighting (resulting in uniform
//   shading — acceptable since dFdx/dFdy is not available in the
//   vertex shader).
// ──────────────────────────────────────────────────────────────────────

// --- Vertex inputs (separate bindings) ---
layout(location = 0) in vec3 inPosition;

#if defined(HAS_NORMALS)
layout(location = 1) in vec3 inNormal;
#endif

#if defined(COLOR_PER_VERTEX)
layout(location = 2) in vec4 inColor;
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
    vec4 camera_pos;      // xyz = world-space camera position
};

layout(binding = 1) uniform Material {
    vec4 u_uniform_color;       // rgba
    vec4 u_light_dir;           // xyz = direction, w = ambient_strength
    vec4 u_light_color;         // xyz = light color, w = diffuse_strength
    vec4 u_specular_params;     // xyz = specular_color, w = shininess
    vec4 u_scalar_params;       // x = scalar_min, y = scalar_max, z = point_size, w = two_sided
    vec4 u_layer_color;         // rgba — per-layer color (wireframe, point, or solid override)
};

// --- Outputs to fragment shader ---
layout(location = 0) out vec3 v_world_pos;

#if defined(SHADING_GOURAUD)
// Gouraud: lighting computed here; pass lit color, no normal needed in frag
layout(location = 1) out vec4 v_lit_color;
#elif defined(SHADING_FLAT)
layout(location = 1) flat out vec3 v_normal;
#else
layout(location = 1) out vec3 v_normal;
#endif

layout(location = 2) out vec4 v_color;

#if defined(COLOR_SCALAR_FIELD)
layout(location = 3) out float v_scalar;
#endif


void main() {
    vec4 world_pos = model * vec4(inPosition, 1.0);
    v_world_pos = world_pos.xyz;
    gl_Position = projection * view * world_pos;

    // Normal
    vec3 N = vec3(0.0, 0.0, 1.0);
#if defined(HAS_NORMALS)
    N = normalize(mat3(normal_matrix) * inNormal);
#endif

    // Base colour / scalar pass-through
#if defined(COLOR_PER_VERTEX)
    v_color = inColor;
#elif defined(COLOR_SCALAR_FIELD)
    v_scalar = inScalar;
    v_color = vec4(1.0);   // placeholder; colormap applied in fragment
#else
    // COLOR_UNIFORM (default)
    v_color = u_uniform_color;
#endif

#if defined(SHADING_GOURAUD)
    // ── Gouraud: compute Blinn-Phong lighting per-vertex ──
    // Two-sided lighting: flip normal to face the camera so both
    // sides receive correct lighting.
    vec3 V = normalize(camera_pos.xyz - v_world_pos);
    bool two_sided = (u_scalar_params.w > 0.5);
    if (two_sided && dot(N, V) < 0.0) {
        N = -N;
    }

    vec3 L = normalize(u_light_dir.xyz);
    vec3 light_col = u_light_color.xyz;
    float ambient = u_light_dir.w;
    float diffuse_str = u_light_color.w;
    float diff = max(dot(N, L), 0.0);

    vec3 H = normalize(L + V);
    float shininess = u_specular_params.w;
    float spec = pow(max(dot(N, H), 0.0), shininess);

    vec3 base = v_color.rgb;
    vec3 result = ambient * base
                + diff * diffuse_str * light_col * base
                + spec * light_col * u_specular_params.xyz;

    v_lit_color = vec4(result, v_color.a);
#else
    // Pass normal to fragment shader for per-fragment lighting
    v_normal = N;
#endif

    // Point size (for point layer pass)
    gl_PointSize = u_scalar_params.z;
}
