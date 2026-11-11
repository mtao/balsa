// Showing data (normals, factors, coordinates) as colour; see
// MeshRenderer::debug_channels().
//
// Output is linear and an sRGB swapchain encodes it; decoding first makes
// the displayed value equal the data value, as in the Khronos glTF Sample
// Viewer's debug views.

vec3 srgb_to_linear(vec3 c) {
    c = clamp(c, 0.0, 1.0);
    return mix(c / 12.92, pow((c + 0.055) / 1.055, vec3(2.4)), step(0.04045, c));
}

// value in [0, 1] per component.
vec4 show_data(vec3 value) { return vec4(srgb_to_linear(value), 1.0); }
vec4 show_data(float value) { return show_data(vec3(value)); }

// A direction, [-1, 1] mapped to [0, 1].
vec4 show_vector(vec3 value) { return show_data(value * 0.5 + 0.5); }
