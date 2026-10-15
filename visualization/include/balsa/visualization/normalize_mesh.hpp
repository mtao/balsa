#pragma once
#include <optional>

#include <balsa/scene_graph/types.hpp>

namespace balsa::scene_graph {
class BasicShading;
} // namespace balsa::scene_graph

namespace balsa::visualization {

// The transform that centres the 3D positions bound by `shading` at the
// origin and scales their largest bounding-box extent to 1, e.g. for an
// Object's local transform:
//   if (auto xf = unit_box_transform(shading)) object.set_from_transform(*xf);
// nullopt if `shading` has no bound 3-component positions.
auto unit_box_transform(const scene_graph::BasicShading &shading)
    -> std::optional<scene_graph::AffineTransformf>;

} // namespace balsa::visualization
