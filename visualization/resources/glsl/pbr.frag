#version 460 core
#extension GL_GOOGLE_include_directive : require
// glTF metallic-roughness surfaces ("balsa.pbr"): the BRDF of glTF 2.0, Appendix B
// (GGX distribution, height-correlated Smith visibility, Schlick Fresnel),
// lit by the scene light plus a small ambient term.  Texture and normal-map
// conventions follow the Khronos glTF Sample Viewer (Apache-2.0).
//
// Defines: as pbr.vert.

layout(location = 0) in vec3 v_world_pos;
layout(location = 1) in vec3 v_normal;
layout(location = 2) in vec4 v_tangent;
layout(location = 3) in vec2 v_texture_coordinates0;
layout(location = 4) in vec2 v_texture_coordinates1;
layout(location = 5) in vec4 v_color;

#include "balsa/scene.glsl"

layout(set = 1, binding = 0) uniform Material {
    vec4 u_base_color;         // linear rgba
    vec4 u_emissive;           // xyz = linear rgb, w = alpha cutoff
    vec4 u_factors;            // metallic, roughness, normal scale, occlusion strength
    vec4 u_texture_coordinate_sets; // of base colour, metallic-roughness, normal, occlusion
    vec4 u_options;            // x = emissive set, y = alpha mode, z = double sided,
                               // w = debug channel (PbrDebugChannel)
};

// Empty slots hold white (normal: a flat normal) placeholders.
layout(set = 1, binding = 1) uniform sampler2D u_base_color_texture;          // sRGB
layout(set = 1, binding = 2) uniform sampler2D u_metallic_roughness_texture;  // G = roughness, B = metallic
layout(set = 1, binding = 3) uniform sampler2D u_normal_texture;              // tangent space
layout(set = 1, binding = 4) uniform sampler2D u_occlusion_texture;           // R
layout(set = 1, binding = 5) uniform sampler2D u_emissive_texture;            // sRGB

layout(location = 0) out vec4 outColor;

const float PI = 3.14159265358979;
// Uniform ambient light until image-based lighting exists.
const float AMBIENT = 0.03;

vec2 texture_coordinates(float set) {
    return set < 0.5 ? v_texture_coordinates0 : v_texture_coordinates1;
}

// Trowbridge-Reitz / GGX normal distribution; alpha = roughness^2.
float D_GGX(float NdotH, float alpha) {
    float a2 = alpha * alpha;
    float f = NdotH * NdotH * (a2 - 1.0) + 1.0;
    return a2 / (PI * f * f);
}

// Height-correlated Smith visibility, V = G / (4 NdotL NdotV).
float V_GGX(float NdotL, float NdotV, float alpha) {
    float a2 = alpha * alpha;
    float ggx_v = NdotL * sqrt(NdotV * NdotV * (1.0 - a2) + a2);
    float ggx_l = NdotV * sqrt(NdotL * NdotL * (1.0 - a2) + a2);
    float sum = ggx_v + ggx_l;
    return sum > 0.0 ? 0.5 / sum : 0.0;
}

vec3 F_Schlick(vec3 f0, float VdotH) {
    return f0 + (vec3(1.0) - f0) * pow(clamp(1.0 - VdotH, 0.0, 1.0), 5.0);
}

// The swapchain encodes to sRGB; decoding first shows data values as is.
vec3 srgb_to_linear(vec3 c) {
    c = clamp(c, 0.0, 1.0);
    return mix(c / 12.92, pow((c + 0.055) / 1.055, vec3(2.4)), step(0.04045, c));
}
vec4 show_data(vec3 value) { return vec4(srgb_to_linear(value), 1.0); }
vec4 show_vector(vec3 value) { return show_data(value * 0.5 + 0.5); }

// Debug channels, matching PbrDebugChannel.
const int DEBUG_FINAL = 0;
const int DEBUG_BASE_COLOR = 1;
const int DEBUG_ALPHA = 2;
const int DEBUG_METALLIC = 3;
const int DEBUG_ROUGHNESS = 4;
const int DEBUG_OCCLUSION = 5;
const int DEBUG_EMISSIVE = 6;
const int DEBUG_SHADING_NORMAL = 7;
const int DEBUG_GEOMETRY_NORMAL = 8;
const int DEBUG_TANGENT = 9;
const int DEBUG_BITANGENT = 10;
const int DEBUG_TEXTURE_COORDINATES0 = 11;
const int DEBUG_TEXTURE_COORDINATES1 = 12;

void main() {
    // ── Base colour and alpha ──
    vec4 base = u_base_color * v_color
              * texture(u_base_color_texture, texture_coordinates(u_texture_coordinate_sets.x));
    int alpha_mode = int(u_options.y + 0.5);
    if (alpha_mode == 1) {                 // MASK
        if (base.a < u_emissive.w) discard;
        base.a = 1.0;
    } else if (alpha_mode == 0) {          // OPAQUE
        base.a = 1.0;
    }

    vec4 mr = texture(u_metallic_roughness_texture, texture_coordinates(u_texture_coordinate_sets.y));
    float metallic = clamp(u_factors.x * mr.b, 0.0, 1.0);
    float roughness = clamp(u_factors.y * mr.g, 0.03, 1.0);
    float alpha = roughness * roughness;

    // ── Normal ──
#if defined(HAS_NORMALS)
    vec3 ng = normalize(v_normal);
#else
    vec3 ng = normalize(cross(dFdx(v_world_pos), dFdy(v_world_pos)));
#endif
    vec2 normal_uv = texture_coordinates(u_texture_coordinate_sets.z);
    vec3 t;
    vec3 b;
#if defined(HAS_TANGENTS)
    t = normalize(v_tangent.xyz - ng * dot(ng, v_tangent.xyz));
    b = cross(ng, t) * v_tangent.w;
#else
    // Tangent frame from screen-space derivatives of position and
    // texture coordinates.
    vec3 uv_dx = dFdx(vec3(normal_uv, 0.0));
    vec3 uv_dy = dFdy(vec3(normal_uv, 0.0));
    float det = uv_dx.s * uv_dy.t - uv_dy.s * uv_dx.t;
    vec3 t_ = (uv_dy.t * dFdx(v_world_pos) - uv_dx.t * dFdy(v_world_pos)) / (det == 0.0 ? 1.0 : det);
    t = t_ - ng * dot(ng, t_);
    t = dot(t, t) > 0.0 ? normalize(t) : normalize(abs(ng.x) < 0.9 ? cross(ng, vec3(1, 0, 0))
                                                                  : cross(ng, vec3(0, 1, 0)));
    b = cross(ng, t);
#endif
    if (!gl_FrontFacing) {
        ng = -ng;
        t = -t;
        b = -b;
    }
    vec3 normal_sample = texture(u_normal_texture, normal_uv).rgb * 2.0 - 1.0;
    normal_sample.xy *= u_factors.z;
    vec3 n = normalize(mat3(t, b, ng) * normal_sample);

    // ── Lighting ──
    vec3 v = normalize(camera_pos.xyz - v_world_pos);
    vec3 l = normalize(light_direction.xyz);
    vec3 h = normalize(l + v);
    float NdotL = clamp(dot(n, l), 0.0, 1.0);
    float NdotV = clamp(abs(dot(n, v)), 1e-4, 1.0);
    float NdotH = clamp(dot(n, h), 0.0, 1.0);
    float VdotH = clamp(dot(v, h), 0.0, 1.0);

    vec3 c_diffuse = mix(base.rgb, vec3(0.0), metallic);
    vec3 f0 = mix(vec3(0.04), base.rgb, metallic);
    vec3 F = F_Schlick(f0, VdotH);
    vec3 diffuse = (vec3(1.0) - F) * c_diffuse / PI;
    vec3 specular = F * D_GGX(NdotH, alpha) * V_GGX(NdotL, NdotV, alpha);
    // Scaled by PI so a white diffuse surface facing the light is white.
    vec3 direct = PI * (diffuse + specular) * light_color.rgb * light_color.w * NdotL;

    float occlusion = texture(u_occlusion_texture, texture_coordinates(u_texture_coordinate_sets.w)).r;
    occlusion = 1.0 + u_factors.w * (occlusion - 1.0);
    vec3 ambient = AMBIENT * base.rgb * occlusion;

    vec3 emissive = u_emissive.rgb
                  * texture(u_emissive_texture, texture_coordinates(u_options.x)).rgb;

    switch (int(u_options.w + 0.5)) {
    case DEBUG_BASE_COLOR: outColor = vec4(base.rgb, 1.0); return;
    case DEBUG_ALPHA: outColor = show_data(vec3(base.a)); return;
    case DEBUG_METALLIC: outColor = show_data(vec3(metallic)); return;
    case DEBUG_ROUGHNESS: outColor = show_data(vec3(roughness)); return;
    case DEBUG_OCCLUSION: outColor = show_data(vec3(occlusion)); return;
    case DEBUG_EMISSIVE: outColor = vec4(emissive, 1.0); return;
    case DEBUG_SHADING_NORMAL: outColor = show_vector(n); return;
    case DEBUG_GEOMETRY_NORMAL: outColor = show_vector(ng); return;
    case DEBUG_TANGENT: outColor = show_vector(t); return;
    case DEBUG_BITANGENT: outColor = show_vector(b); return;
    case DEBUG_TEXTURE_COORDINATES0: outColor = show_data(vec3(fract(v_texture_coordinates0), 0.0)); return;
    case DEBUG_TEXTURE_COORDINATES1: outColor = show_data(vec3(fract(v_texture_coordinates1), 0.0)); return;
    default: break;
    }

    // Linear; an sRGB swapchain encodes it.
    outColor = vec4(direct + ambient + emissive, base.a);
}
