// Descriptor set 0, shared by every program (see ShaderProgram.hpp).

layout(set = 0, binding = 0) uniform Transforms {
    mat4 model;
    mat4 view;
    mat4 projection;
    mat4 normal_matrix;   // transpose(inverse(model))
    vec4 camera_pos;      // xyz = world-space camera position
};

layout(set = 0, binding = 1) uniform SceneLighting {
    vec4 light_direction; // xyz = world-space direction to the light
    vec4 light_color;     // xyz = colour, w = 1 if the light is on, else 0
};
