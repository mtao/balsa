#pragma once
#include "Extension.hpp"
#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace balsa::visualization::scene_graph::gltf {

// A node in the node hierarchy. When the node contains skin, all
// mesh.primitives MUST contain JOINTS_0 and WEIGHTS_0 attributes. A node MAY
// have either a matrix or any combination of translation/rotation/scale (TRS)
// properties. TRS properties are converted to matrices and postmultiplied in
// the T * R * S order to compose the transformation matrix; first the scale is
// applied to the vertices, then the rotation, and then the translation. If none
// are provided, the transform is the identity. When a node is targeted for
// animation (referenced by an animation.channel.target), matrix MUST NOT be
// present.
struct [[=object]] Node {
    // The index of the camera referenced by this node.
    std::optional<int64_t> camera;
    // The indices of this node’s children.
    std::vector<int64_t> children;
    // The index of the skin referenced by this node.
    std::optional<int64_t> skin;
    // A floating-point 4x4 transformation matrix stored in column-major
    // order.
    [[=json::manual_write]] std::array<double, 16> matrix = {1.0, 0.0, 0.0, 0.0, //
                                     0.0, 1.0, 0.0, 0.0, //
                                     0.0, 0.0, 1.0, 0.0, //
                                     0.0, 0.0, 0.0, 1.0};
    // The index of the mesh in this node.
    std::optional<int64_t> mesh;
    // The node’s unit quaternion rotation in the order (x, y, z, w), where w is
    // the scalar.
    [[=json::manual_write]] std::array<double, 4> rotation = {0.0, 0.0, 0.0, 1.0};
    // The node’s non-uniform scale, given as the scaling factors along the x,
    // y, and z axes.
    [[=json::manual_write]] std::array<double, 3> scale = {1.0, 1.0, 1.0};
    // The node’s translation along the x, y, and z axes.
    [[=json::manual_write]] std::array<double, 3> translation = {0.0, 0.0, 0.0};
    // The weights of the instantiated morph target. The number of array
    // elements MUST match the number of morph targets of the referenced mesh.
    // When defined, mesh MUST also be defined.
    std::vector<double> weights;
    // The user-defined name of this object.
    std::optional<std::string> name;
    // JSON object with extension-specific objects.
    Extension extensions;
    // Application-specific data.
    Extras extras;
};

auto to_json(nlohmann::json &j, const Node &v) -> void;
auto from_json(const nlohmann::json &j, Node &v) -> void;

} // namespace balsa::visualization::scene_graph::gltf
