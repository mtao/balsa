#pragma once
#include "Extension.hpp"
#include <array>
#include <cstdint>
#include <optional>
#include <string>

namespace balsa::visualization::scene_graph::gltf {

// Reference to a texture.
struct [[=object]] TextureInfo {
    // The index of the texture.
    [[=json::required]] int64_t index;
    // The set index of texture’s TEXCOORD attribute used for texture coordinate
    // mapping.
    [[=json::skip_default]] int64_t texCoord = 0;
    // JSON object with extension-specific objects.
    Extension extensions;
    // Application-specific data.
    Extras extras;
};

// Reference to a texture.
struct [[=object]] MaterialNormalTextureInfo {
    // The index of the texture.
    [[=json::required]] int64_t index;
    // The set index of texture’s TEXCOORD attribute used for texture coordinate
    // mapping.
    [[=json::skip_default]] int64_t texCoord = 0;
    // The scalar parameter applied to each normal vector of the normal texture.
    [[=json::skip_default]] double scale = 1.0;
    // JSON object with extension-specific objects.
    Extension extensions;
    // Application-specific data.
    Extras extras;
};

// Reference to a texture.
struct [[=object]] MaterialOcclusionTextureInfo {
    // The index of the texture.
    [[=json::required]] int64_t index;
    // The set index of texture’s TEXCOORD attribute used for texture coordinate
    // mapping.
    [[=json::skip_default]] int64_t texCoord = 0;
    // A scalar multiplier controlling the amount of occlusion applied.
    [[=json::skip_default]] double strength = 1.0;
    // JSON object with extension-specific objects.
    Extension extensions;
    // Application-specific data.
    Extras extras;
};

// A set of parameter values that are used to define the metallic-roughness
// material model from Physically-Based Rendering (PBR) methodology.
struct [[=object]] MaterialPBRMetallicRoughness {
    // The factors for the base color of the material.
    [[=json::skip_default]] std::array<double, 4> baseColorFactor = {1.0, 1.0, 1.0, 1.0};
    // The base color texture.
    std::optional<TextureInfo> baseColorTexture;
    // The factor for the metalness of the material.
    [[=json::skip_default]] double metallicFactor = 1.0;
    // The factor for the roughness of the material.
    [[=json::skip_default]] double roughnessFactor = 1.0;
    // The metallic-roughness texture.
    std::optional<TextureInfo> metallicRoughnessTexture;
    // JSON object with extension-specific objects.
    Extension extensions;
    // Application-specific data.
    Extras extras;
};

// The alpha rendering mode of the material.
enum class AlphaMode {
    OPAQUE,
    MASK,
    BLEND,
};

// The material appearance of a primitive.
struct [[=object]] Material {
    // The user-defined name of this object.
    std::optional<std::string> name;
    // JSON object with extension-specific objects.
    Extension extensions;
    // Application-specific data.
    Extras extras;
    // A set of parameter values that are used to define the metallic-roughness
    // material model from Physically Based Rendering (PBR) methodology. When
    // undefined, all the default values of pbrMetallicRoughness MUST apply.
    std::optional<MaterialPBRMetallicRoughness> pbrMetallicRoughness;
    // The tangent space normal texture.
    std::optional<MaterialNormalTextureInfo> normalTexture;
    // The occlusion texture.
    std::optional<MaterialOcclusionTextureInfo> occlusionTexture;
    // The emissive texture.
    std::optional<TextureInfo> emissiveTexture;
    // The factors for the emissive color of the material.
    [[=json::skip_default]] std::array<double, 3> emissiveFactor = {0.0, 0.0, 0.0};
    // The alpha rendering mode of the material.
    [[=json::skip_default]] AlphaMode alphaMode = AlphaMode::OPAQUE;
    // The alpha cutoff value of the material.
    [[=json::manual_write]] double alphaCutoff = 0.5;
    // Specifies whether the material is double sided.
    [[=json::skip_default]] bool doubleSided = false;
};

auto to_json(nlohmann::json &j, const AlphaMode &v) -> void;
auto from_json(const nlohmann::json &j, AlphaMode &v) -> void;
auto to_json(nlohmann::json &j, const TextureInfo &v) -> void;
auto from_json(const nlohmann::json &j, TextureInfo &v) -> void;
auto to_json(nlohmann::json &j, const MaterialNormalTextureInfo &v) -> void;
auto from_json(const nlohmann::json &j, MaterialNormalTextureInfo &v) -> void;
auto to_json(nlohmann::json &j, const MaterialOcclusionTextureInfo &v) -> void;
auto from_json(const nlohmann::json &j, MaterialOcclusionTextureInfo &v) -> void;
auto to_json(nlohmann::json &j, const MaterialPBRMetallicRoughness &v) -> void;
auto from_json(const nlohmann::json &j, MaterialPBRMetallicRoughness &v) -> void;
auto to_json(nlohmann::json &j, const Material &v) -> void;
auto from_json(const nlohmann::json &j, Material &v) -> void;

} // namespace balsa::visualization::scene_graph::gltf
