#pragma once
#include "Texture.hpp"
#include "types.hpp"

#include <cstdint>
#include <optional>
#include <string>

namespace balsa::scene_graph {

enum class AlphaMode : uint8_t {
    Opaque,
    // Opaque where alpha >= alpha_cutoff, discarded elsewhere.
    Mask,
    Blend,
};

// A metallic-roughness material (glTF 2.0's core material).  Shared between
// the layers that draw with it, so edits apply to all of them.  Factors
// multiply their texture, when present.
class Material {
  public:
    std::string name;

    // Linear RGBA; its texture is sRGB.
    Vec4f base_color{1.0f, 1.0f, 1.0f, 1.0f};
    std::optional<MaterialTexture> base_color_texture;

    float metallic = 1.0f;
    float roughness = 1.0f;
    // Roughness in green, metallic in blue; linear.
    std::optional<MaterialTexture> metallic_roughness_texture;

    // Tangent-space normals; linear.
    std::optional<MaterialTexture> normal_texture;
    float normal_scale = 1.0f;

    // Occlusion in red; linear.
    std::optional<MaterialTexture> occlusion_texture;
    float occlusion_strength = 1.0f;

    // Linear RGB; its texture is sRGB.
    Vec3f emissive{0.0f, 0.0f, 0.0f};
    std::optional<MaterialTexture> emissive_texture;

    AlphaMode alpha_mode = AlphaMode::Opaque;
    float alpha_cutoff = 0.5f;
    // Draws back faces too, with flipped normals.
    bool double_sided = false;
};

} // namespace balsa::scene_graph
