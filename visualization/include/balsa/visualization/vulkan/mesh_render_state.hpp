#if !defined(BALSA_VISUALIZATION_VULKAN_MESH_RENDER_STATE_HPP)
#define BALSA_VISUALIZATION_VULKAN_MESH_RENDER_STATE_HPP

#include <cstdint>
#include <string>

#include "balsa/scene_graph/render_layers.hpp"

namespace balsa::visualization::vulkan {

// Shading model determines how lighting is evaluated.
//   Flat:    provoking-vertex normal used for entire triangle (flat interpolation)
//   Gouraud: lighting computed per-vertex, interpolated color across face
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
//   FromAttribute:    per-vertex normal attribute (smooth shading)
//   ComputedInShader: flat normals computed from dFdx/dFdy in fragment shader
//   None:             no normals — shading disabled (unlit)
enum class NormalSource : uint8_t { FromAttribute,
                                    ComputedInShader,
                                    None };

// Which faces the rasterizer discards.
//   None:  draw all faces (default — no culling)
//   Back:  cull back faces (show exterior only)
//   Front: cull front faces (show interior only)
enum class CullMode : uint8_t { None,
                                Back,
                                Front };

// Shared source of truth for mesh rendering parameters.
// Read/written by both ImGui and Qt UI panels.
struct MeshRenderState {
    ShadingModel shading = ShadingModel::Phong;
    ColorSource color_source = ColorSource::Uniform;
    NormalSource normal_source = NormalSource::FromAttribute;

    // Face culling (rasterizer).
    CullMode cull_mode = CullMode::None;

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

    // ── Constraint enforcement ──────────────────────────────────────
    //
    // Enforce invariants between shading model, normal source, and
    // actual normal data availability.  Call after any mutation of
    // shading or normal_source, and at load time.
    //
    // Rules:
    //   - FromAttribute requires the mesh to actually have normals.
    //     If !has_normals, downgrade to ComputedInShader.
    //   - Phong and Gouraud require per-vertex normals (FromAttribute).
    //     When normal_source != FromAttribute, downgrade to Flat.
    //   - NormalSource::None disables lighting entirely — shading model
    //     is ignored by the shader, so no change needed.
    //
    // Returns true if any field was modified.
    bool constrain(bool has_normals) {
        bool modified = false;

        // Can't use FromAttribute without actual normal data.
        if (normal_source == NormalSource::FromAttribute && !has_normals) {
            normal_source = NormalSource::ComputedInShader;
            modified = true;
        }

        // Phong/Gouraud need per-vertex normals from an attribute.
        // ComputedInShader gives flat per-triangle normals via dFdx/dFdy,
        // which makes Phong interpolation meaningless and Gouraud
        // per-vertex lighting incorrect.
        if (normal_source == NormalSource::ComputedInShader
            && (shading == ShadingModel::Phong || shading == ShadingModel::Gouraud)) {
            shading = ShadingModel::Flat;
            modified = true;
        }

        return modified;
    }
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
