#pragma once
#include "Extension.hpp"
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace balsa::visualization::scene_graph::gltf {

// The root nodes of a scene.
struct [[=object]] Scene {
    // The indices of each root node.
    std::vector<int64_t> nodes;
    // The user-defined name of this object.
    std::optional<std::string> name;
    // JSON object with extension-specific objects.
    Extension extensions;
    // Application-specific data.
    Extras extras;
};

auto to_json(nlohmann::json &j, const Scene &v) -> void;
auto from_json(const nlohmann::json &j, Scene &v) -> void;

} // namespace balsa::visualization::scene_graph::gltf
