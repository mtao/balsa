#if !defined(BALSA_VISUALIZATION_VULKAN_MESH_RENDER_STATE_HPP)
#define BALSA_VISUALIZATION_VULKAN_MESH_RENDER_STATE_HPP

#include <cstdint>
#include <string>

#include "balsa/scene_graph/render_layers.hpp"

namespace balsa::visualization::vulkan {

// Shading model determines how lighting is evaluated.
//   Flat:    provoking-vertex normal used for entire triangle (flat interpolation)
//   Gouraud: lighting computed per-vertex, color interpolated across face
//            (currently degrades to Phong; vertex-shader lighting is a future addition)
//   Phong:   normal interpolated across face, lighting evaluated per-fragment
enum class ShadingModel : uint8_t { Flat,
                                    Gouraud,
                                    Phong };

// Where fragment color comes from.
//   Uniform:     single color for the whole mesh (from uniform_color)
//   PerVertex:   per-vertex RGB attribute
//   ScalarField: per-vertex scalar mapped through a colormap
enum class ColorSource : uint8_t { Uniform,
                                   PerVertex,
                                   ScalarField };

// Where surface normals come from.
//   FromAttribute:    per-vertex normal attribute
//   ComputedInShader: geometry shader computes flat face normals on GPU (future)
//   None:             no normals — shading disabled
enum class NormalSource : uint8_t { FromAttribute,
                                    ComputedInShader,
                                    None };

// Shared source of truth for mesh rendering parameters.
// Read/written by both ImGui and Qt UI panels.
struct MeshRenderState {
    ShadingModel shading = ShadingModel::Phong;
    ColorSource color_source = ColorSource::Uniform;
    NormalSource normal_source = NormalSource::FromAttribute;

    // ── Render layers ───────────────────────────────────────────────
    // Three composable layers (solid, wireframe, points), each with
    // independent enable flag and color.  Default: solid + wireframe
    // enabled, points disabled.
    scene_graph::RenderLayers layers;

    // ── Material ────────────────────────────────────────────────────
    // Per-mesh material response to scene lighting.  The light
    // source (direction, color) comes from the scene's Light feature.
    scene_graph::MaterialProperties material;

    // Uniform color (when color_source == Uniform).  RGBA.
    float uniform_color[4] = { 0.8f, 0.8f, 0.8f, 1.0f };

    // Colormap name for scalar field (matches Qt resource alias, e.g. "MATLAB_jet").
    std::string colormap_name = "MATLAB_jet";
    float scalar_min = 0.0f;
    float scalar_max = 1.0f;

    // Light both sides of faces.
    bool two_sided = true;
};

// ── ResolvedLightState ──────────────────────────────────────────────
//
// World-space lighting parameters resolved once per frame by
// MeshScene from the scene graph's Light features.  Passed to each
// VulkanMeshDrawable so that meshes pack their MaterialUBO from
// both scene lighting and per-mesh MaterialProperties.

struct ResolvedLightState {
    // World-space light direction (normalised).
    float light_dir[3] = { 0.577f, 0.577f, 0.577f };
    // Light color (multiplied with diffuse/specular).
    float light_color[3] = { 1.0f, 1.0f, 1.0f };
};

}// namespace balsa::visualization::vulkan

#endif
