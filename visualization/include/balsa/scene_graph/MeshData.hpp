#if !defined(BALSA_SCENE_GRAPH_MESH_DATA_HPP)
#define BALSA_SCENE_GRAPH_MESH_DATA_HPP

#include <cstdint>
#include <optional>
#include <span>

#include "AbstractFeature.hpp"
#include "types.hpp"
#include "balsa/visualization/vulkan/mesh_render_state.hpp"

#include <quiver/Mesh.hpp>
#include <quiver/attributes/AttributeManager.hpp>

namespace balsa::scene_graph {

// ── MeshData ────────────────────────────────────────────────────────
//
// Feature that holds CPU-side mesh geometry and appearance parameters.
//
// Uses quiver's AttributeManager as the backing store for vertex/index
// data and an optional quiver::Mesh<2> for triangle mesh topology.
// When triangle indices are set, edges are automatically derived from
// the topology's 1-skeleton so that wireframe rendering works without
// explicit edge data.
//
// Each mutator bumps a version counter so that the rendering layer
// can detect when GPU buffers need re-uploading.
//
// The render_state member holds shading, material, and display
// parameters.  It uses the existing MeshRenderState struct from the
// Vulkan layer (which is renderer-agnostic despite its namespace).

class MeshData : public AbstractFeature {
  public:
    using RenderState = visualization::vulkan::MeshRenderState;

    MeshData();

    // ── Geometry mutators ───────────────────────────────────────────
    // Each setter copies the data and bumps the version counter.

    void set_positions(std::span<const Vec3f> positions);
    void set_normals(std::span<const Vec3f> normals);
    void set_triangle_indices(std::span<const uint32_t> indices);
    void set_edge_indices(std::span<const uint32_t> indices);
    void set_vertex_colors(std::span<const Vec4f> colors);
    void set_scalar_field(std::span<const float> scalars);

    // ── Geometry accessors ──────────────────────────────────────────

    std::span<const Vec3f> positions() const;
    std::span<const Vec3f> normals() const;
    std::span<const uint32_t> triangle_indices() const;
    std::span<const uint32_t> edge_indices() const;
    std::span<const Vec4f> vertex_colors() const;
    std::span<const float> scalar_field() const;

    bool has_positions() const;
    bool has_normals() const;
    bool has_triangle_indices() const;
    bool has_edge_indices() const;
    bool has_vertex_colors() const;
    bool has_scalar_field() const;

    std::size_t vertex_count() const;
    std::size_t triangle_count() const;
    std::size_t edge_count() const;

    // ── Topology ────────────────────────────────────────────────────
    // Access the quiver Mesh<2> topology (built from triangle indices).

    bool has_topology() const { return _topology.has_value(); }
    const quiver::Mesh<2> &topology() const { return *_topology; }

    // ── Dirty tracking ──────────────────────────────────────────────
    // The version is bumped on every mutator call.  The rendering
    // layer compares against its last-synced version to decide
    // whether GPU buffers need updating.

    uint64_t version() const { return _version; }

    // ── Render state ────────────────────────────────────────────────

    RenderState &render_state() { return _render_state; }
    const RenderState &render_state() const { return _render_state; }

  private:
    // Attribute backing store.
    quiver::attributes::AttributeManager _attrs;

    // Cached handles for fast access (set once in constructor).
    quiver::attributes::AttributeHandle<Vec3f> _h_positions;
    quiver::attributes::AttributeHandle<Vec3f> _h_normals;
    quiver::attributes::AttributeHandle<uint32_t> _h_triangle_indices;
    quiver::attributes::AttributeHandle<uint32_t> _h_edge_indices;
    quiver::attributes::AttributeHandle<Vec4f> _h_vertex_colors;
    quiver::attributes::AttributeHandle<float> _h_scalar_field;

    // Triangle mesh topology — built from triangle indices.
    // When present, edges are derived from the 1-skeleton.
    std::optional<quiver::Mesh<2>> _topology;

    // Whether edges were explicitly set by the user (vs auto-derived).
    bool _explicit_edges = false;

    // Derive edge indices from the Mesh<2> topology's 1-skeleton.
    void derive_edges_from_topology();

    RenderState _render_state;
    uint64_t _version = 0;
};

}// namespace balsa::scene_graph

#endif
