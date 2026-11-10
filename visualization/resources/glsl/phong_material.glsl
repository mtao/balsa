// The "balsa.phong" material (set 1) and its lighting.

layout(set = 1, binding = 0) uniform Material {
    vec4 u_color;          // uniform colour
    vec4 u_response;       // ambient, diffuse and specular strength, shininess
    vec4 u_scalar_range;   // x = min, y = max, z = two-sided (> 0.5)
    vec4 u_scalar_select;  // x = mode (0 component, 1 p-norm),
                           // y = component or p, z = component count
};

// The scene light's diffuse (with ambient) and specular terms at a surface
// with normal N seen along V; unlit when the light is off.
void phong_terms(vec3 N, vec3 V, out vec3 diffuse, out vec3 specular) {
    if (u_scalar_range.z > 0.5 && dot(N, V) < 0.0) {
        N = -N;   // two-sided: face the viewer
    }
    if (light_color.w < 0.5) {
        diffuse = vec3(1.0);
        specular = vec3(0.0);
        return;
    }
    vec3 L = normalize(light_direction.xyz);
    vec3 H = normalize(L + V);
    diffuse = vec3(u_response.x) + u_response.y * max(dot(N, L), 0.0) * light_color.rgb;
    specular = u_response.z * pow(max(dot(N, H), 0.0), u_response.w) * light_color.rgb;
}
