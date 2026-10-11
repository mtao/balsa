#include "balsa/scene_graph/mesh_io.hpp"

#include "balsa/scene_graph/MeshData.hpp"

#include <balsa/geometry/triangle_mesh/read_obj.hpp>

#include <cstdint>
#include <vector>

namespace balsa::scene_graph {

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

auto load_mesh(MeshData &mesh_data, const std::filesystem::path &path)
    -> std::expected<void, std::string> {
    if (!std::filesystem::exists(path)) {
        return std::unexpected("file not found: " + path.string());
    }
    auto ext = path.extension().string();
    if (ext != ".obj" && ext != ".OBJ") {
        return std::unexpected("unsupported mesh format: " + ext);
    }

    auto obj = geometry::triangle_mesh::read_objF(path);
    const auto &P = obj.position;
    if (P.vertices.cols() == 0) {
        return std::unexpected("no vertices in " + path.string());
    }

    mesh_data.clear();
    mesh_data.add_attribute("vertex_positions",
                            AttributeDomain::Vertex,
                            3,
                            std::span<const float>(
                                flatten_columns<float>(P.vertices)));

    // Only use normals if they are indexed like the positions; OBJ
    // allows independent normal indices which do not map per-vertex.
    const auto &N = obj.normal;
    if (N.vertices.cols() == P.vertices.cols()
        && N.triangles.cols() == P.triangles.cols()
        && flatten_columns<uint32_t>(N.triangles)
               == flatten_columns<uint32_t>(P.triangles)) {
        mesh_data.add_attribute("vertex_normals",
                                AttributeDomain::Vertex,
                                3,
                                std::span<const float>(
                                    flatten_columns<float>(N.vertices)));
    }

    mesh_data.set_triangle_indices(flatten_columns<uint32_t>(P.triangles));
    return {};
}

} // namespace balsa::scene_graph
