#pragma once
#include <balsa/geometry/triangle_mesh/read_obj.hpp>

namespace balsa::scene_graph {
class MeshGeometry;
}

namespace balsa::visualization {

// Fill a MeshGeometry (replacing its contents) from a parsed OBJ mesh.
//
// Adds "vertex_positions", plus "vertex_normals" when the normals are
// indexed identically to the positions, and the triangle indices.
// Parsing itself is balsa::geometry's job (read_objF / read_objD).
auto to_mesh_geometry(const geometry::triangle_mesh::OBJMesh<float, 3> &obj,
                      scene_graph::MeshGeometry &geometry) -> void;

} // namespace balsa::visualization
