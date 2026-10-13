#pragma once
#include <cstdint>
#include <memory>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "AbstractFeature.hpp"
#include "MeshAttribute.hpp"

#include <zipper/storage/layout_types.hpp>
#include <zipper/types.hpp>

namespace balsa::scene_graph {

// ── MeshGeometry ────────────────────────────────────────────────────
//
// Feature holding a mesh's data as plain buffers: named, typed attributes
// on vertices/edges/faces, and triangle/edge index buffers.  It knows
// nothing about rendering; shading features on the same Object (e.g.
// BasicShading) choose which attributes they use.
//
// Edges are derived from triangles unless set explicitly.  version() is
// bumped on every change, so consumers know when to re-read.

class MeshGeometry : public AbstractFeature {
  public:
    MeshGeometry();

    // ── Attributes ──────────────────────────────────────────────────
    //
    // Add (or replace, if the name exists) an attribute.  data is a
    // rank-1 mdspan (one scalar per element) or a rank-2 mdspan with
    // extents (components, elements) in any layout; e.g. N x 3
    // row-major positions are a 3 x N layout_left mdspan over the same
    // memory.  Replacing keeps the attribute's identity, so existing
    // handles see the new values.  Returns an invalid handle if the
    // shape is unusable.
    template <AttributeValue T,
              typename Extents,
              typename Layout,
              typename Accessor>
        requires(Extents::rank() == 1 || Extents::rank() == 2)
    auto add_attribute(std::string name,
                       AttributeDomain domain,
                       ::zipper::mdspan<T, Extents, Layout, Accessor> data)
        -> MeshAttributeHandle;
    // Convenience: flat element-major data (any contiguous range),
    // component_count values per element.
    template <std::ranges::contiguous_range R>
        requires std::ranges::sized_range<R>
    auto add_attribute(std::string name,
                       AttributeDomain domain,
                       uint8_t component_count,
                       const R &data) -> MeshAttributeHandle;

    auto remove_attribute(std::string_view name) -> bool;

    // Random-access view of MeshAttributeHandle.
    [[nodiscard]] auto attributes() const {
        return _attributes
               | std::views::transform(
                   [](const std::shared_ptr<MeshAttribute> &a) {
                       return MeshAttributeHandle(a);
                   });
    }
    // Invalid handle if not found.
    [[nodiscard]] auto find_attribute(std::string_view name) const
        -> MeshAttributeHandle;
    // Whether attr is one of this geometry's (current) attributes.
    [[nodiscard]] auto owns(const MeshAttributeHandle &attr) const -> bool;

    // ── Topology ────────────────────────────────────────────────────

    // Element vertex indices as any rank-2 integral mdspan, in either
    // orientation: N x 3 (one row per triangle) or 3 x N (one column per
    // triangle).  Orientation is taken from whichever extent is 3 (rows
    // win if both are).  Any layout is accepted; element-ordered memory
    // (N x 3 row-major or 3 x N column-major) is copied directly.
    // Returns false (and changes nothing) if neither extent is 3.
    template <typename T, typename Extents, typename Layout, typename Accessor>
        requires(Extents::rank() == 2)
    auto set_triangle_indices(
        ::zipper::mdspan<T, Extents, Layout, Accessor> indices) -> bool;
    // Explicit edges override edges derived from triangles.
    // Same conventions as set_triangle_indices with 2 indices per edge.
    template <typename T, typename Extents, typename Layout, typename Accessor>
        requires(Extents::rank() == 2)
    auto
        set_edge_indices(::zipper::mdspan<T, Extents, Layout, Accessor> indices)
            -> bool;

    [[nodiscard]] auto triangle_indices() const -> std::span<const uint32_t> {
        return _tri_indices;
    }
    [[nodiscard]] auto edge_indices() const -> std::span<const uint32_t> {
        return _edge_indices;
    }
    [[nodiscard]] auto has_triangle_indices() const -> bool {
        return !_tri_indices.empty();
    }
    [[nodiscard]] auto has_edge_indices() const -> bool {
        return !_edge_indices.empty();
    }
    [[nodiscard]] auto triangle_count() const -> std::size_t {
        return _tri_indices.size() / 3;
    }
    [[nodiscard]] auto edge_count() const -> std::size_t {
        return _edge_indices.size() / 2;
    }

    // Remove all attributes and topology.
    auto clear() -> void;

    // ── Dirty tracking ──────────────────────────────────────────────

    [[nodiscard]] auto version() const -> uint64_t { return _version; }

  private:
    auto insert_attribute(MeshAttribute attr) -> MeshAttributeHandle;
    auto derive_edges() -> void;
    auto assign_triangles(std::vector<uint32_t> flat) -> void;
    auto assign_edges(std::vector<uint32_t> flat) -> void;

    std::vector<std::shared_ptr<MeshAttribute>> _attributes;
    std::vector<uint32_t> _tri_indices;
    std::vector<uint32_t> _edge_indices;
    bool _explicit_edges = false;
    uint64_t _version = 0;
};

} // namespace balsa::scene_graph

#include "MeshGeometry.hxx"
