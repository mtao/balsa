#pragma once
#include <balsa/geometry/triangle_mesh/read_obj.hpp>
#include <balsa/scene_graph/MeshGeometry.hpp>

namespace balsa::visualization {

// A MeshGeometry holding a parsed OBJ mesh: "vertex_positions", plus
// "vertex_normals" when the normals are indexed identically to the
// positions, and the triangles.  Parsing itself is balsa::geometry's job
// (read_objF / read_objD), e.g.
//   auto geometry = to_mesh_geometry(geometry::triangle_mesh::read_objF(path));
auto to_mesh_geometry(const geometry::triangle_mesh::OBJMesh<float, 3> &obj)
    -> scene_graph::MeshGeometry;

} // namespace balsa::visualization
