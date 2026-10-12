#pragma once
#include <balsa/geometry/triangle_mesh/read_obj.hpp>

namespace balsa::scene_graph {
class MeshData;
}

namespace balsa::visualization {

// Fill MeshData (replacing its contents) from a parsed OBJ mesh.
//
// Adds "vertex_positions", plus "vertex_normals" when the normals are
// indexed identically to the positions, and the triangle indices.
// Parsing itself is balsa::geometry's job (read_objF / read_objD).
auto to_mesh_data(const geometry::triangle_mesh::OBJMesh<float, 3> &obj,
                  scene_graph::MeshData &mesh_data) -> void;

} // namespace balsa::visualization
