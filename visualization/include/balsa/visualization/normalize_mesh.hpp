#pragma once
#include <optional>

#include <balsa/scene_graph/types.hpp>

namespace balsa::scene_graph {
class MeshAttribute;
} // namespace balsa::scene_graph

namespace balsa::visualization {

// The transform that centres 3-component `positions` at the origin and
// scales their largest bounding-box extent to 1, e.g. for an Object's
// local transform:
//   if (auto xf = unit_box_transform(*positions)) object.set_from_transform(*xf);
// nullopt if positions are not 3-component or are empty.
auto unit_box_transform(const scene_graph::MeshAttribute &positions)
    -> std::optional<scene_graph::AffineTransformf>;

} // namespace balsa::visualization
