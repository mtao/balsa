#pragma once
#include "Extension.hpp"
#include <cstdint>
#include <optional>
#include <string>

namespace balsa::visualization::scene_graph::gltf {

// Image data used to create a texture. Image MAY be referenced by an URI (or
// IRI) or a buffer view index.
struct [[=object]] Image {
    // The URI (or IRI) of the image.
    std::optional<std::string> uri;
    // The image’s media type. This field MUST be defined when bufferView is
    // defined.
    std::optional<std::string> mimeType;
    // The index of the bufferView that contains the image. This field MUST NOT
    // be defined when uri is defined.
    std::optional<int64_t> bufferView;
    // The user-defined name of this object.
    std::optional<std::string> name;
    // JSON object with extension-specific objects.
    Extension extensions;
    // Application-specific data.
    Extras extras;
};

auto to_json(nlohmann::json &j, const Image &v) -> void;
auto from_json(const nlohmann::json &j, Image &v) -> void;

} // namespace balsa::visualization::scene_graph::gltf
