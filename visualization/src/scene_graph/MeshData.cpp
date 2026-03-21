#include "balsa/scene_graph/MeshData.hpp"

namespace balsa::scene_graph {

void MeshData::set_positions(std::span<const Vec3f> positions) {
    _positions.assign(positions.begin(), positions.end());
    ++_version;
}

void MeshData::set_normals(std::span<const Vec3f> normals) {
    _normals.assign(normals.begin(), normals.end());
    ++_version;
}

void MeshData::set_triangle_indices(std::span<const uint32_t> indices) {
    _triangle_indices.assign(indices.begin(), indices.end());
    ++_version;
}

void MeshData::set_edge_indices(std::span<const uint32_t> indices) {
    _edge_indices.assign(indices.begin(), indices.end());
    ++_version;
}

void MeshData::set_vertex_colors(std::span<const Vec4f> colors) {
    _vertex_colors.assign(colors.begin(), colors.end());
    ++_version;
}

void MeshData::set_scalar_field(std::span<const float> scalars) {
    _scalar_field.assign(scalars.begin(), scalars.end());
    ++_version;
}

}// namespace balsa::scene_graph
