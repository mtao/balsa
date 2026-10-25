#pragma once
#include "Extension.hpp"
#include <cstdint>
#include <optional>
#include <string>

namespace balsa::visualization::scene_graph::gltf {

// Magnification filter.
enum class MagFilter : int64_t {
    NEAREST = 9728,
    LINEAR = 9729,
};

// Minification filter.
enum class MinFilter : int64_t {
    NEAREST = 9728,
    LINEAR = 9729,
    NEAREST_MIPMAP_NEAREST = 9984,
    LINEAR_MIPMAP_NEAREST = 9985,
    NEAREST_MIPMAP_LINEAR = 9986,
    LINEAR_MIPMAP_LINEAR = 9987,
};

// Texture coordinate wrapping mode.
enum class WrapMode : int64_t {
    CLAMP_TO_EDGE = 33071,
    MIRRORED_REPEAT = 33648,
    REPEAT = 10497,
};

// Texture sampler properties for filtering and wrapping modes.
struct [[=object]] Sampler {
    // Magnification filter.
    std::optional<MagFilter> magFilter;
    // Minification filter.
    std::optional<MinFilter> minFilter;
    // S (U) wrapping mode.
    [[=json::skip_default]] WrapMode wrapS = WrapMode::REPEAT;
    // T (V) wrapping mode.
    [[=json::skip_default]] WrapMode wrapT = WrapMode::REPEAT;
    // The user-defined name of this object.
    std::optional<std::string> name;
    // JSON object with extension-specific objects.
    Extension extensions;
    // Application-specific data.
    Extras extras;
};

auto to_json(nlohmann::json &j, const Sampler &v) -> void;
auto from_json(const nlohmann::json &j, Sampler &v) -> void;

} // namespace balsa::visualization::scene_graph::gltf
