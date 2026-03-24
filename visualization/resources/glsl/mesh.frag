#version 460 core

#ifdef WIREFRAME_OVERLAY
#extension GL_EXT_fragment_shader_barycentric : require
#endif

// ──────────────────────────────────────────────────────────────────────
// Mesh uber-fragment-shader
//
// Preprocessor defines (set by MeshShader):
//   HAS_NORMALS          — vertex normal attribute is present
//   SHADING_FLAT / SHADING_PHONG / SHADING_GOURAUD
//   COLOR_UNIFORM / COLOR_PER_VERTEX / COLOR_SCALAR_FIELD
//   RENDER_WIREFRAME — bypass lighting, output layer_color directly
//   RENDER_POINTS    — bypass lighting, output layer_color directly
//   WIREFRAME_OVERLAY — merged solid+wireframe: overlay wireframe edges
//                       over the lit solid fill using gl_BaryCoordEXT
//                       (requires VK_KHR_fragment_shader_barycentric)
//
// Normal handling:
//   SHADING_FLAT always uses the geometric face normal computed from
//   screen-space partial derivatives (dFdx/dFdy) of the world position.
//   This gives exact per-face normals without requiring a geometry
//   shader or per-vertex normal attributes.
//
//   SHADING_PHONG uses the interpolated per-vertex normal from the
//   vertex shader (requires HAS_NORMALS).  If HAS_NORMALS is not
//   defined, it falls back to the dFdx/dFdy face normal.
//
//   SHADING_GOURAUD computes lighting per-vertex and interpolates
//   the lit color — no normal is needed in the fragment shader.
//
// When COLOR_SCALAR_FIELD is defined, the colormap function
//   vec4 colormap(float x)
// is prepended to this source by MeshShader before compilation.
// ──────────────────────────────────────────────────────────────────────

// --- Inputs from vertex shader ---
layout(location = 0) in vec3 v_world_pos;

#if defined(SHADING_GOURAUD)
layout(location = 1) in vec4 v_lit_color;
#elif defined(SHADING_FLAT)
// Flat shading: the vertex shader outputs with 'flat' qualifier.
// We must match that here.  The value is ignored when we compute
// the face normal from dFdx/dFdy, but the interface must agree.
layout(location = 1) flat in vec3 v_normal;
#else
// Phong (smooth interpolation) or fallback
layout(location = 1) in vec3 v_normal;
#endif

layout(location = 2) in vec4 v_color;

#if defined(COLOR_SCALAR_FIELD)
layout(location = 3) in float v_scalar;
#endif

// --- UBOs ---
layout(binding = 0) uniform Transforms {
    mat4 model;
    mat4 view;
    mat4 projection;
    mat4 normal_matrix;
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

// --- Output ---
layout(location = 0) out vec4 outColor;


void main() {
#if defined(RENDER_WIREFRAME) || defined(RENDER_POINTS)
    // Wireframe / point mode: use layer color directly, no lighting
    outColor = u_layer_color;
#else
    // ── 1. Determine base colour and alpha ──
    vec3 base_color;
    float alpha = v_color.a;

#if defined(COLOR_SCALAR_FIELD)
    float range = max(u_scalar_params.y - u_scalar_params.x, 1e-6);
    float t = clamp((v_scalar - u_scalar_params.x) / range, 0.0, 1.0);
    base_color = colormap(t).rgb;
#else
    base_color = v_color.rgb;
#endif

    // ── 2. Lighting ──
#if defined(SHADING_GOURAUD)
    // Gouraud: lighting was computed per-vertex in the vertex shader.
    // v_lit_color contains the interpolated lit result.
    vec3 result = v_lit_color.rgb;
    alpha = v_lit_color.a;

    // Two-sided: for back faces, re-light with flipped normal is not
    // possible in Gouraud mode.  We approximate by using the absolute
    // value of the interpolated diffuse (already baked into v_lit_color).
    // This is a known Gouraud limitation.

    outColor = vec4(result, alpha);
#elif defined(SHADING_FLAT) || defined(SHADING_PHONG)

    // ── Determine surface normal ──
    //
    // SHADING_FLAT: always use the geometric face normal derived from
    // screen-space partial derivatives.  This gives exact per-triangle
    // flat shading regardless of whether vertex normals exist.
    //
    // SHADING_PHONG + HAS_NORMALS: use the smoothly interpolated
    // per-vertex normal for per-fragment lighting.
    //
    // SHADING_PHONG without HAS_NORMALS: fall back to the dFdx/dFdy
    // face normal (equivalent to flat shading but still per-fragment).

    vec3 N;
#if defined(SHADING_PHONG) && defined(HAS_NORMALS)
    N = normalize(v_normal);
#else
    // Geometric face normal from screen-space derivatives of world pos.
    // cross(dFdx, dFdy) gives a vector perpendicular to the triangle
    // in world space.  The sign depends on winding / screen orientation.
    vec3 dpdx = dFdx(v_world_pos);
    vec3 dpdy = dFdy(v_world_pos);
    N = normalize(cross(dpdx, dpdy));
#endif

    // View vector (needed for two-sided flip and specular)
    vec3 V = normalize(camera_pos.xyz - v_world_pos);

    // Two-sided lighting: ensure the normal faces the viewer.
    // This works correctly regardless of winding order, face culling,
    // or whether the normal came from a vertex attribute or dFdx/dFdy.
    bool two_sided = (u_scalar_params.w > 0.5);
    if (two_sided && dot(N, V) < 0.0) {
        N = -N;
    }

    vec3 L = normalize(u_light_dir.xyz);
    vec3 light_col = u_light_color.xyz;
    float ambient = u_light_dir.w;
    float diffuse_str = u_light_color.w;
    float diff = max(dot(N, L), 0.0);

    // Blinn-Phong specular
    vec3 H = normalize(L + V);
    float shininess = u_specular_params.w;
    float spec = pow(max(dot(N, H), 0.0), shininess);

    vec3 result = ambient * base_color
                + diff * diffuse_str * light_col * base_color
                + spec * light_col * u_specular_params.xyz;

    outColor = vec4(result, alpha);
#else
    // No shading — unlit output
    outColor = vec4(base_color, alpha);
#endif

    // ── 3. Wireframe overlay (merged solid+wireframe pass) ──
    //
    // When WIREFRAME_OVERLAY is defined, we have access to gl_BaryCoordEXT
    // from VK_KHR_fragment_shader_barycentric.  This gives per-fragment
    // barycentric coordinates without any vertex duplication.
    //
    // We compute the distance to the nearest triangle edge in barycentric
    // space and blend the wireframe color over the already-computed solid
    // color.  The result is smooth, anti-aliased wireframe lines whose
    // width is controlled by u_scalar_params.z.
    //
    // UBO packing for merged pass:
    //   u_layer_color    = wireframe color (rgba)
    //   u_scalar_params.z = wireframe width (in approximate pixels)
#ifdef WIREFRAME_OVERLAY
    {
        vec3 bary = gl_BaryCoordEXT;

        // Distance to nearest edge in barycentric space
        float d = min(min(bary.x, bary.y), bary.z);

        // Use fwidth for screen-space anti-aliasing.
        // wire_width controls how many pixels wide the wireframe lines are.
        float wire_width = u_scalar_params.z;
        float fw = fwidth(d);
        float edge = smoothstep(0.0, fw * wire_width, d);

        // edge = 0 on the wire, 1 in the interior.
        // Blend wireframe color (u_layer_color) over solid color.
        outColor = mix(u_layer_color, outColor, edge);
    }
#endif

#endif // RENDER_WIREFRAME || RENDER_POINTS
}
