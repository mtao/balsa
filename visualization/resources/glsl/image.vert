#version 460 core

layout(binding = 0) uniform Transforms {
    mat4 mvp;
};

// Two triangles forming a quad. A real quad is required because pan/zoom
// transforms would expose the diagonal edge of an oversized fullscreen
// triangle.

layout(location = 0) out vec2 v_uv;

void main() {
    const vec2 positions[6] = vec2[](
        vec2(-1.0, -1.0), vec2(1.0, -1.0), vec2(1.0, 1.0),
        vec2(-1.0, -1.0), vec2(1.0, 1.0), vec2(-1.0, 1.0));
    const vec2 uvs[6] = vec2[](
        vec2(0.0, 0.0), vec2(1.0, 0.0), vec2(1.0, 1.0),
        vec2(0.0, 0.0), vec2(1.0, 1.0), vec2(0.0, 1.0));
    vec2 ndc = positions[gl_VertexIndex];
    v_uv = uvs[gl_VertexIndex];
    gl_Position = mvp * vec4(ndc, 0.0, 1.0);
}
