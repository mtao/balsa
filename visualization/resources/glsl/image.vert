#version 460 core

layout(binding = 0) uniform Transforms {
    mat4 mvp;
};

// Fullscreen triangle technique: 3 vertices, no vertex buffer needed.
// Vertex IDs 0, 1, 2 produce a triangle that covers the entire clip
// space [-1, 1]^2.  UVs map to [0, 1]^2 for texture sampling.
//
// When an MVP is provided (non-identity), the quad corners are
// transformed for pan/zoom support.

layout(location = 0) out vec2 v_uv;

void main() {
    // Positions for a fullscreen triangle covering [-1, 1]^2:
    //   Vertex 0: (-1, -1)
    //   Vertex 1: ( 3, -1)
    //   Vertex 2: (-1,  3)
    vec2 pos = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
    v_uv = pos * 0.5;
    // Map from [0,2] -> [-1,1]
    vec2 ndc = pos - 1.0;
    gl_Position = mvp * vec4(ndc, 0.0, 1.0);
}
