#include "balsa/scene_graph/MeshData.hpp"

#include <quiver/attributes/IncidentFaceIndices.hpp>

namespace balsa::scene_graph {

MeshData::MeshData()
  : _h_positions(_attrs.create<Vec3f>("positions")), _h_normals(_attrs.create<Vec3f>("normals")), _h_triangle_indices(_attrs.create<uint32_t>("triangle_indices")), _h_edge_indices(_attrs.create<uint32_t>("edge_indices")), _h_vertex_colors(_attrs.create<Vec4f>("vertex_colors")), _h_scalar_field(_attrs.create<float>("scalar_field")) {}

// ── Geometry mutators ───────────────────────────────────────────────

void MeshData::set_positions(std::span<const Vec3f> positions) {
    _h_positions.attribute().mutable_data().assign(
      positions.begin(), positions.end());
    ++_version;
}

void MeshData::set_normals(std::span<const Vec3f> normals) {
    _h_normals.attribute().mutable_data().assign(
      normals.begin(), normals.end());
    ++_version;
}

void MeshData::set_triangle_indices(std::span<const uint32_t> indices) {
    _h_triangle_indices.attribute().mutable_data().assign(
      indices.begin(), indices.end());

    // Build quiver Mesh<2> topology from triangle indices.
    std::size_t n_tris = indices.size() / 3;
    if (n_tris > 0) {
        std::vector<std::array<int64_t, 3>> qtris(n_tris);
        for (std::size_t j = 0; j < n_tris; ++j) {
            qtris[j] = {
                static_cast<int64_t>(indices[j * 3 + 0]),
                static_cast<int64_t>(indices[j * 3 + 1]),
                static_cast<int64_t>(indices[j * 3 + 2])
            };
        }
        _topology = quiver::Mesh<2>::from_vertex_indices(qtris);

        // Auto-derive edges unless the user explicitly set them.
        if (!_explicit_edges) {
            derive_edges_from_topology();
        }
    } else {
        _topology.reset();
        if (!_explicit_edges) {
            _h_edge_indices.attribute().mutable_data().clear();
        }
    }

    ++_version;
}

void MeshData::set_edge_indices(std::span<const uint32_t> indices) {
    _h_edge_indices.attribute().mutable_data().assign(
      indices.begin(), indices.end());
    _explicit_edges = true;
    ++_version;
}

void MeshData::set_vertex_colors(std::span<const Vec4f> colors) {
    _h_vertex_colors.attribute().mutable_data().assign(
      colors.begin(), colors.end());
    ++_version;
}

void MeshData::set_scalar_field(std::span<const float> scalars) {
    _h_scalar_field.attribute().mutable_data().assign(
      scalars.begin(), scalars.end());
    ++_version;
}

// ── Geometry accessors ──────────────────────────────────────────────

std::span<const Vec3f> MeshData::positions() const {
    const auto &d = _h_positions.attribute().data();
    return { d.data(), d.size() };
}

std::span<const Vec3f> MeshData::normals() const {
    const auto &d = _h_normals.attribute().data();
    return { d.data(), d.size() };
}

std::span<const uint32_t> MeshData::triangle_indices() const {
    const auto &d = _h_triangle_indices.attribute().data();
    return { d.data(), d.size() };
}

std::span<const uint32_t> MeshData::edge_indices() const {
    const auto &d = _h_edge_indices.attribute().data();
    return { d.data(), d.size() };
}

std::span<const Vec4f> MeshData::vertex_colors() const {
    const auto &d = _h_vertex_colors.attribute().data();
    return { d.data(), d.size() };
}

std::span<const float> MeshData::scalar_field() const {
    const auto &d = _h_scalar_field.attribute().data();
    return { d.data(), d.size() };
}

bool MeshData::has_positions() const { return _h_positions.size() > 0; }
bool MeshData::has_normals() const { return _h_normals.size() > 0; }
bool MeshData::has_triangle_indices() const { return _h_triangle_indices.size() > 0; }
bool MeshData::has_edge_indices() const { return _h_edge_indices.size() > 0; }
bool MeshData::has_vertex_colors() const { return _h_vertex_colors.size() > 0; }
bool MeshData::has_scalar_field() const { return _h_scalar_field.size() > 0; }

std::size_t MeshData::vertex_count() const { return _h_positions.size(); }
std::size_t MeshData::triangle_count() const { return _h_triangle_indices.size() / 3; }
std::size_t MeshData::edge_count() const { return _h_edge_indices.size() / 2; }

// ── Private: edge derivation ────────────────────────────────────────

void MeshData::derive_edges_from_topology() {
    if (!_topology) return;

    auto &mesh = *_topology;
    const auto &edge_skel = mesh.build_skeleton<1>();
    const auto &vert_skel = mesh.skeleton<0>();

    quiver::attributes::IncidentFaceIndices<1, 0, 2> edge_verts(
      edge_skel, vert_skel);
    std::size_t ne = edge_skel.size();

    auto &edge_data = _h_edge_indices.attribute().mutable_data();
    edge_data.resize(ne * 2);
    for (std::size_t e = 0; e < ne; ++e) {
        auto [v0, v1] = edge_verts.get_indices(e);
        edge_data[e * 2 + 0] = static_cast<uint32_t>(v0);
        edge_data[e * 2 + 1] = static_cast<uint32_t>(v1);
    }
}

}// namespace balsa::scene_graph
