#if !defined(BALSA_SCENE_GRAPH_RENDER_LAYERS_HPP)
#define BALSA_SCENE_GRAPH_RENDER_LAYERS_HPP

namespace balsa::scene_graph {

// ── Render Layers ───────────────────────────────────────────────────
//
// Plain data structs describing how a mesh is rasterized.  A mesh
// has exactly three layers — solid, wireframe, points — any
// combination of which may be enabled simultaneously.
//
// These structs are renderer-agnostic: no Vulkan, OpenGL, or Qt
// dependencies.  The rendering backend reads them; the UI backends
// (ImGui, Qt) provide controls that mutate them.

// Filled-triangle pass.
struct SolidLayer {
    float color[4] = { 0.8f, 0.8f, 0.8f, 1.0f };// face color (RGBA)
    bool enabled = true;
};

// Edge-line pass.
struct WireframeLayer {
    float color[4] = { 0.0f, 0.0f, 0.0f, 1.0f };// wire color (RGBA)
    float width = 1.5f;
    bool enabled = true;
};

// Point-cloud pass.
struct PointLayer {
    float color[4] = { 1.0f, 0.0f, 0.0f, 1.0f };// point color (RGBA)
    float size = 3.0f;
    bool enabled = false;// off by default
};

// Fixed triple of layers — one per primitive topology.
struct RenderLayers {
    SolidLayer solid;
    WireframeLayer wireframe;
    PointLayer points;
};

// ── Material Properties ─────────────────────────────────────────────
//
// Per-mesh material response to scene lighting.  The light source
// itself (direction, color) lives on the scene's Light feature;
// these fields control how the mesh reacts to that light.

struct MaterialProperties {
    float ambient_strength = 0.15f;
    float specular_strength = 0.5f;
    float shininess = 32.0f;
};

}// namespace balsa::scene_graph

#endif
