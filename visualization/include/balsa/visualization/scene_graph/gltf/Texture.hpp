#pragma once
#include "Extension.hpp"
#include <cstdint>
#include <optional>
#include <string>

namespace balsa::visualization::scene_graph::gltf {

// A texture and its sampler.
struct [[=object]] Texture {
    // The index of the sampler used by this texture. When undefined, a sampler
    // with repeat wrapping and auto filtering SHOULD be used.
    std::optional<int64_t> sampler;
    // The index of the image used by this texture. When undefined, an extension
    // or other mechanism SHOULD supply an alternate texture source, otherwise
    // behavior is undefined.
    std::optional<int64_t> source;
    // The user-defined name of this object.
    std::optional<std::string> name;
    // JSON object with extension-specific objects.
    Extension extensions;
    // Application-specific data.
    Extras extras;
};

auto to_json(nlohmann::json &j, const Texture &v) -> void;
auto from_json(const nlohmann::json &j, Texture &v) -> void;

} // namespace balsa::visualization::scene_graph::gltf
