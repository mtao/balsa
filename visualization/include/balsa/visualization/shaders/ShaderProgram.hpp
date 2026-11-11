#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include <balsa/catalog/Catalog.hpp>
#include <balsa/scene_graph/MeshRole.hpp>
#include <balsa/scene_graph/Texture.hpp>

namespace balsa::visualization::shaders {

// A vertex input of a program, read from the attribute bound to `role`.
struct VertexStream {
    scene_graph::MeshRole role;
    uint32_t location;
    bool required = false;
    // Defined when the stream is bound (optional streams).
    std::string define = {};
};

// What an unbound texture slot samples.
enum class TextureFallback : uint8_t {
    White,      // leaves factors unchanged
    FlatNormal, // a tangent-space normal along the surface normal
};

struct TextureSlot {
    std::string name;
    scene_graph::ColorSpace color_space = scene_graph::ColorSpace::Linear;
    TextureFallback fallback = TextureFallback::White;
};

// Largest material block a program may declare.
inline constexpr uint32_t max_material_size = 256;

// A shader program: its sources and the interface renderers fill.
//
// Descriptor set 0 is shared by all programs: the transforms (binding 0)
// and the scene lighting (binding 1), declared in "balsa/scene.glsl".  Set 1
// is the program's material: a std140 uniform block of material_size bytes
// (binding 0) and `textures` (bindings 1, 2, ...).
struct ShaderProgramDefinition {
    std::string id;
    std::string label;
    // Names in the ShaderLibrary.
    std::string vertex_shader;
    std::string fragment_shader;
    std::vector<VertexStream> streams;
    uint32_t material_size = 0;
    std::vector<TextureSlot> textures;
};

using ShaderProgram = catalog::Entry<ShaderProgramDefinition>;

// Default catalog, with the built-ins below.
auto shader_programs() -> catalog::Catalog<ShaderProgramDefinition> &;

namespace programs {
    // Blinn-Phong surfaces (flat, Gouraud or per-fragment), uniform colour
    // or a colormapped scalar: "balsa.phong".
    auto phong() -> ShaderProgram;
    // glTF metallic-roughness surfaces: "balsa.gltf_metallic_roughness".
    auto gltf_metallic_roughness() -> ShaderProgram;
    // Any attribute shown as an unlit colour: "balsa.attribute_color".
    auto attribute_color() -> ShaderProgram;
    // Unlit lines of one colour: "balsa.unlit_lines".
    auto unlit_lines() -> ShaderProgram;
    // Unlit points of one colour and size: "balsa.unlit_points".
    auto unlit_points() -> ShaderProgram;
} // namespace programs

} // namespace balsa::visualization::shaders
