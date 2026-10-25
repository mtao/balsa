#pragma once
#include "Extension.hpp"
#include <optional>
#include <string>

namespace balsa::visualization::scene_graph::gltf {

// Metadata about the glTF asset.
struct [[=object]] Asset {
    // A copyright message suitable for display to credit the content creator.
    std::optional<std::string> copyright;
    // Tool that generated this glTF model.  Useful for debugging.
    std::optional<std::string> generator;
    // The glTF version in the form of <major>.<minor> that this asset targets.
    [[=json::required]] std::string version;
    // The minimum glTF version in the form of <major>.<minor> that this asset
    // targets. This property MUST NOT be greater than the asset version.
    std::optional<std::string> minVersion;
    // JSON object with extension-specific objects.
    Extension extensions;
    // Application-specific data.
    Extras extras;
};

auto to_json(nlohmann::json &j, const Asset &v) -> void;
auto from_json(const nlohmann::json &j, Asset &v) -> void;

} // namespace balsa::visualization::scene_graph::gltf
