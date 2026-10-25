#pragma once
#include "Extension.hpp"
#include <cstdint>
#include <optional>
#include <string>

namespace balsa::visualization::scene_graph::gltf {

// A buffer points to binary geometry, animation, or skins.
struct [[=object]] Buffer {
    // The URI (or IRI) of the buffer.
    std::optional<std::string> uri;
    // The length of the buffer in bytes.
    [[=json::required]] int64_t byteLength;
    // The user-defined name of this object.
    std::optional<std::string> name;
    // JSON object with extension-specific objects.
    Extension extensions;
    // Application-specific data.
    Extras extras;
};

auto to_json(nlohmann::json &j, const Buffer &v) -> void;
auto from_json(const nlohmann::json &j, Buffer &v) -> void;

} // namespace balsa::visualization::scene_graph::gltf
