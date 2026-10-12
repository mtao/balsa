#include "balsa/visualization/mesh_conversion.hpp"

#include "balsa/scene_graph/MeshData.hpp"

#include <algorithm>

namespace balsa::visualization {

auto to_mesh_data(const geometry::triangle_mesh::OBJMesh<float, 3> &obj,
                  scene_graph::MeshData &mesh_data) -> void {
    using scene_graph::AttributeDomain;
    const auto &P = obj.position;
    const auto &N = obj.normal;

    // Vertices and triangles are column-per-element matrices, which is
    // exactly the (components, elements) shape MeshData expects.
    mesh_data.clear();
    mesh_data.add_attribute("vertex_positions",
                            AttributeDomain::Vertex,
                            P.vertices.expression().as_mdspan());

    // OBJ allows independent normal indices, which do not map
    // per-vertex; only use normals indexed like the positions.
    if (N.vertices.cols() == P.vertices.cols()
        && std::ranges::equal(N.triangles.expression().as_std_span(),
                              P.triangles.expression().as_std_span())) {
        mesh_data.add_attribute("vertex_normals",
                                AttributeDomain::Vertex,
                                N.vertices.expression().as_mdspan());
    }

    mesh_data.set_triangle_indices(P.triangles.expression().as_mdspan());
}

} // namespace balsa::visualization
