#include "balsa/visualization/mesh_conversion.hpp"

#include "balsa/scene_graph/MeshData.hpp"

#include <cstdint>
#include <span>
#include <vector>

namespace balsa::visualization {

namespace {

    // Flatten a column-vector matrix (D rows x N cols) into an
    // element-major buffer: x0 y0 z0 x1 y1 z1 ...
    template <typename T, typename Matrix>
    auto flatten_columns(const Matrix &M) -> std::vector<T> {
        std::vector<T> out;
        out.reserve(M.rows() * M.cols());
        for (std::size_t j = 0; j < M.cols(); ++j) {
            for (std::size_t i = 0; i < M.rows(); ++i) {
                out.push_back(static_cast<T>(M(i, j)));
            }
        }
        return out;
    }

} // namespace

auto to_mesh_data(const geometry::triangle_mesh::OBJMesh<float, 3> &obj,
                  scene_graph::MeshData &mesh_data) -> void {
    using scene_graph::AttributeDomain;
    const auto &P = obj.position;
    const auto &N = obj.normal;

    mesh_data.clear();
    mesh_data.add_attribute(
        "vertex_positions",
        AttributeDomain::Vertex,
        3,
        std::span<const float>(flatten_columns<float>(P.vertices)));

    // OBJ allows independent normal indices, which do not map
    // per-vertex; only use normals indexed like the positions.
    auto tris = flatten_columns<uint32_t>(P.triangles);
    if (N.vertices.cols() == P.vertices.cols()
        && flatten_columns<uint32_t>(N.triangles) == tris) {
        mesh_data.add_attribute(
            "vertex_normals",
            AttributeDomain::Vertex,
            3,
            std::span<const float>(flatten_columns<float>(N.vertices)));
    }

    mesh_data.set_triangle_indices(
        ::zipper::mdspan<const uint32_t, ::zipper::dextents<2>>(
            tris.data(), tris.size() / 3, 3));
}

} // namespace balsa::visualization
