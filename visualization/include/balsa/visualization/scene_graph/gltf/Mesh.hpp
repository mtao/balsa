#pragma once
#include "Extension.hpp"
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace balsa::visualization::scene_graph::gltf {

// The topology type of primitives to render.
enum class PrimitiveMode : int64_t {
    POINTS = 0,
    LINES = 1,
    LINE_LOOP = 2,
    LINE_STRIP = 3,
    TRIANGLES = 4,
    TRIANGLE_STRIP = 5,
    TRIANGLE_FAN = 6,
};

// Geometry to be rendered with the given material.
struct [[=object]] MeshPrimitive {
    // A plain JSON object, where each key corresponds to a mesh attribute
    // semantic and each value is the index of the accessor containing
    // attribute’s data.
    [[=json::required]] std::map<std::string, int64_t> attributes;
    // The index of the accessor that contains the vertex indices.
    std::optional<int64_t> indices;
    // The index of the material to apply to this primitive when rendering.
    std::optional<int64_t> material;
    // The topology type of primitives to render.
    [[=json::skip_default]] PrimitiveMode mode = PrimitiveMode::TRIANGLES;
    // An array of morph targets.
    std::vector<std::map<std::string, int64_t>> targets;
    // JSON object with extension-specific objects.
    Extension extensions;
    // Application-specific data.
    Extras extras;
};

// A set of primitives to be rendered. Its global transform is defined by a node
// that references it.
struct [[=object]] Mesh {
    // An array of primitives, each defining geometry to be rendered.
    [[=json::required]] std::vector<MeshPrimitive> primitives;
    // Array of weights to be applied to the morph targets. The number of array
    // elements MUST match the number of morph targets.
    std::vector<double> weights;
    // The user-defined name of this object.
    std::optional<std::string> name;
    // JSON object with extension-specific objects.
    Extension extensions;
    // Application-specific data.
    Extras extras;
};

auto to_json(nlohmann::json &j, const MeshPrimitive &v) -> void;
auto from_json(const nlohmann::json &j, MeshPrimitive &v) -> void;
auto to_json(nlohmann::json &j, const Mesh &v) -> void;
auto from_json(const nlohmann::json &j, Mesh &v) -> void;

} // namespace balsa::visualization::scene_graph::gltf
