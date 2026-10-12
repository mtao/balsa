#pragma once

namespace balsa::scene_graph {
class Object;
class MeshData;
} // namespace balsa::scene_graph

namespace balsa::visualization {

// Set `object`'s local transform so that the mesh's 3D positions are
// centered at the origin and their largest bounding-box extent is 1.
// The source data is untouched; only the Object transform changes.
//
// Returns false (leaving the transform untouched) if `mesh_data` has no
// tightly packed 3-component float positions.
auto normalize_to_unit_box(scene_graph::Object &object,
                           const scene_graph::MeshData &mesh_data) -> bool;

} // namespace balsa::visualization
