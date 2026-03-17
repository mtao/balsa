#if !defined(BALSA_VISUALIZATION_VULKAN_MESH_RENDER_STATE_HPP)
#define BALSA_VISUALIZATION_VULKAN_MESH_RENDER_STATE_HPP

#include <cstdint>
#include <string>

namespace balsa::visualization::vulkan {

// Shading model determines how lighting is evaluated.
//   Flat:    provoking-vertex normal used for entire triangle (flat interpolation)
//   Gouraud: lighting computed per-vertex, color interpolated across face
//            (currently degrades to Phong; vertex-shader lighting is a future addition)
//   Phong:   normal interpolated across face, lighting evaluated per-fragment
enum class ShadingModel : uint8_t { Flat,
                                    Gouraud,
                                    Phong };

// How geometry is rasterized.
//   Solid:          filled triangles
//   Wireframe:      edge-only (line draw with edge index buffer)
//   Points:         point cloud
//   SolidWireframe: solid + wireframe overlay via geometry shader (future)
enum class RenderMode : uint8_t { Solid,
                                  Wireframe,
                                  Points,
                                  SolidWireframe };

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
    RenderMode render_mode = RenderMode::Solid;
    ColorSource color_source = ColorSource::Uniform;
    NormalSource normal_source = NormalSource::FromAttribute;

    // Uniform color (when color_source == Uniform).  RGBA.
    float uniform_color[4] = { 0.8f, 0.8f, 0.8f, 1.0f };

    // Colormap name for scalar field (matches Qt resource alias, e.g. "MATLAB_jet").
    std::string colormap_name = "MATLAB_jet";
    float scalar_min = 0.0f;
    float scalar_max = 1.0f;

    // Directional light (world-space, should be normalised).
    float light_dir[3] = { 0.577f, 0.577f, 0.577f };
    float ambient_strength = 0.15f;
    float specular_strength = 0.5f;
    float shininess = 32.0f;

    // Wireframe overlay / standalone wireframe colour.
    float wireframe_color[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
    float wireframe_width = 1.5f;

    // Point rendering size.
    float point_size = 3.0f;

    // Light both sides of faces.
    bool two_sided = true;
};

}// namespace balsa::visualization::vulkan

#endif
