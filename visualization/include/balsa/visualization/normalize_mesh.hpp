#pragma once
#include <optional>

#include <balsa/geometry/BoundingBox.hpp>
#include <balsa/scene_graph/types.hpp>

namespace balsa::scene_graph {
class MeshAttribute;
class Object;
} // namespace balsa::scene_graph

namespace balsa::visualization {

// Transform centring `box` at the origin with largest extent 1.
auto unit_box_transform(const geometry::BoundingBox<float, 3> &box)
    -> std::optional<scene_graph::AffineTransformf>;

// The same for the bounding box of 3-component positions.
auto unit_box_transform(const scene_graph::MeshAttribute &positions)
    -> std::optional<scene_graph::AffineTransformf>;

// Bounding box of every mesh's positions below `root`, in root's
// local space.
auto local_bounding_box(const scene_graph::Object &root)
    -> geometry::BoundingBox<float, 3>;

// The same in world space.
auto world_bounding_box(const scene_graph::Object &root)
    -> geometry::BoundingBox<float, 3>;

} // namespace balsa::visualization
