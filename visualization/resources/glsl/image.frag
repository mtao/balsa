#version 460 core

layout(binding = 1) uniform ImageParams {
    vec4 tone_params;   // x=exposure, y=gamma, z=channel_mode, w=unused
    vec4 image_size;    // x=width, y=height, z=1/width, w=1/height
};

layout(binding = 2) uniform sampler2D u_image;

layout(location = 0) in vec2 v_uv;
layout(location = 0) out vec4 outColor;

void main() {
    vec4 texel = texture(u_image, v_uv);

    // Exposure (EV stops)
    float exposure = tone_params.x;
    texel.rgb *= exp2(exposure);

    // Gamma correction
    float gamma = tone_params.y;
    texel.rgb = pow(max(texel.rgb, vec3(0.0)), vec3(1.0 / gamma));

    // Channel isolation
    int mode = int(tone_params.z);
    if (mode == 1)      outColor = vec4(texel.rrr, 1.0);  // Red
    else if (mode == 2) outColor = vec4(texel.ggg, 1.0);  // Green
    else if (mode == 3) outColor = vec4(texel.bbb, 1.0);  // Blue
    else if (mode == 4) outColor = vec4(texel.aaa, 1.0);  // Alpha
    else if (mode == 5) {                                   // Luminance
        float lum = dot(texel.rgb, vec3(0.2126, 0.7152, 0.0722));
        outColor = vec4(vec3(lum), 1.0);
    }
    else outColor = vec4(texel.rgb, texel.a);              // RGBA
}
