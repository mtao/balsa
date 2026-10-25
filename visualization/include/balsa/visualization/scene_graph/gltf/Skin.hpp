#pragma once
#include "Extension.hpp"
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace balsa::visualization::scene_graph::gltf {

// Joints and matrices defining a skin.
struct [[=object]] Skin {
    // The index of the accessor containing the floating-point 4x4 inverse-bind
    // matrices.
    std::optional<int64_t> inverseBindMatrices;
    // The index of the node used as a skeleton root.
    std::optional<int64_t> skeleton;
    // Indices of skeleton nodes, used as joints in this skin.
    [[=json::required]] std::vector<int64_t> joints;
    // The user-defined name of this object.
    std::optional<std::string> name;
    // JSON object with extension-specific objects.
    Extension extensions;
    // Application-specific data.
    Extras extras;
};

auto to_json(nlohmann::json &j, const Skin &v) -> void;
auto from_json(const nlohmann::json &j, Skin &v) -> void;

} // namespace balsa::visualization::scene_graph::gltf
