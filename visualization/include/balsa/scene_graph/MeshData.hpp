#if !defined(BALSA_SCENE_GRAPH_MESH_DATA_HPP)
#define BALSA_SCENE_GRAPH_MESH_DATA_HPP

#include <cstdint>
#include <span>
#include <vector>

#include "AbstractFeature.hpp"
#include "types.hpp"
#include "balsa/visualization/vulkan/mesh_render_state.hpp"

namespace balsa::scene_graph {

// ── MeshData ────────────────────────────────────────────────────────
//
// Feature that holds CPU-side mesh geometry and appearance parameters.
//
// Owns copies of vertex/index data.  Each mutator bumps a version
// counter so that the rendering layer can detect when GPU buffers
// need re-uploading.
//
// The render_state member holds shading, material, and display
// parameters.  It uses the existing MeshRenderState struct from the
// Vulkan layer (which is renderer-agnostic despite its namespace).

class MeshData : public AbstractFeature {
  public:
    using RenderState = visualization::vulkan::MeshRenderState;

    MeshData() = default;

    // ── Geometry mutators ───────────────────────────────────────────
    // Each setter copies the data and bumps the version counter.

    void set_positions(std::span<const Vec3f> positions);
    void set_normals(std::span<const Vec3f> normals);
    void set_triangle_indices(std::span<const uint32_t> indices);
    void set_edge_indices(std::span<const uint32_t> indices);
    void set_vertex_colors(std::span<const Vec4f> colors);
    void set_scalar_field(std::span<const float> scalars);

    // ── Geometry accessors ──────────────────────────────────────────

    std::span<const Vec3f> positions() const { return _positions; }
    std::span<const Vec3f> normals() const { return _normals; }
    std::span<const uint32_t> triangle_indices() const { return _triangle_indices; }
    std::span<const uint32_t> edge_indices() const { return _edge_indices; }
    std::span<const Vec4f> vertex_colors() const { return _vertex_colors; }
    std::span<const float> scalar_field() const { return _scalar_field; }

    bool has_positions() const { return !_positions.empty(); }
    bool has_normals() const { return !_normals.empty(); }
    bool has_triangle_indices() const { return !_triangle_indices.empty(); }
    bool has_edge_indices() const { return !_edge_indices.empty(); }
    bool has_vertex_colors() const { return !_vertex_colors.empty(); }
    bool has_scalar_field() const { return !_scalar_field.empty(); }

    std::size_t vertex_count() const { return _positions.size(); }
    std::size_t triangle_count() const { return _triangle_indices.size() / 3; }
    std::size_t edge_count() const { return _edge_indices.size() / 2; }

    // ── Dirty tracking ──────────────────────────────────────────────
    // The version is bumped on every mutator call.  The rendering
    // layer compares against its last-synced version to decide
    // whether GPU buffers need updating.

    uint64_t version() const { return _version; }

    // ── Render state ────────────────────────────────────────────────

    RenderState &render_state() { return _render_state; }
    const RenderState &render_state() const { return _render_state; }

  private:
    std::vector<Vec3f> _positions;
    std::vector<Vec3f> _normals;
    std::vector<uint32_t> _triangle_indices;
    std::vector<uint32_t> _edge_indices;
    std::vector<Vec4f> _vertex_colors;
    std::vector<float> _scalar_field;

    RenderState _render_state;
    uint64_t _version = 0;
};

}// namespace balsa::scene_graph

#endif
