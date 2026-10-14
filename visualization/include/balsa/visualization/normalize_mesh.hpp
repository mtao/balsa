#pragma once

namespace balsa::scene_graph {
class Object;
class BasicShading;
} // namespace balsa::scene_graph

namespace balsa::visualization {

// Set `object`'s local transform so that the 3D positions bound by
// `shading` are centered at the origin and their largest bounding-box
// extent is 1.  The source data is untouched; only the Object transform
// changes.
//
// Returns false (leaving the transform untouched) if `shading` has no
// bound 3-component positions.
auto normalize_to_unit_box(scene_graph::Object &object,
                           const scene_graph::BasicShading &shading) -> bool;

} // namespace balsa::visualization
