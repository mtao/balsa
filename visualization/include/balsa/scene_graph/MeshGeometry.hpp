#pragma once
#include <cstdint>
#include <memory>
#include <optional>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "AbstractFeature.hpp"
#include "MeshAttribute.hpp"

#include <zipper/DataArray.hpp>
#include <zipper/storage/layout_types.hpp>
#include <zipper/types.hpp>

namespace balsa::scene_graph {

// ── MeshGeometry ────────────────────────────────────────────────────
//
// Feature holding a mesh's data as named, typed attributes on vertices,
// edges and faces.  It knows nothing about rendering; shading features on
// the same Object (e.g. BasicShading) choose which attributes they use.
//
// Topology is stored as attributes too: ".face_vertices" (Face domain,
// 3 x uint32 per triangle) and ".edge_vertices" (Edge domain, 2 x uint32
// per edge), so the number of faces/edges is the count of those
// attributes.  Names starting with '.' are reserved for MeshGeometry:
// topology changes only through set_triangle_indices/set_edge_indices.
// Edges are derived from triangles unless set explicitly.
//
// version() is bumped on every change, so consumers know when to re-read.

class MeshGeometry : public AbstractFeature {
  public:
    /// Vertex indices of K-vertex elements (K = 3 triangles, 2 edges):
    /// extents (K, count), one column per element, so memory is
    /// element-major (a0 b0 c0 a1 b1 c1 ...).
    template <std::size_t K>
    using element_indices_view = MeshAttribute::const_view_type<uint32_t, K>;

    /// Attributes holding the topology.
    static constexpr std::string_view face_vertices_name = ".face_vertices";
    static constexpr std::string_view edge_vertices_name = ".edge_vertices";
    /// Names starting with '.' are reserved for MeshGeometry's own
    /// attributes (the topology).
    [[nodiscard]] static constexpr auto is_reserved_name(std::string_view name)
        -> bool {
        return name.starts_with('.');
    }

    MeshGeometry();

    // ── Attributes ──────────────────────────────────────────────────
    //
    // Add (or replace, if the name exists) an attribute.  data is a
    // rank-1 mdspan (one scalar per element) or a rank-2 mdspan with
    // extents (components, elements) in any layout; e.g. N x 3
    // row-major positions are a 3 x N layout_left mdspan over the same
    // memory.  Replacing keeps the attribute's identity, so existing
    // handles see the new values.  Returns an invalid handle if the
    // shape is unusable or the name is reserved.  Edge/face attributes
    // whose count differs from the edge/face count are kept with a
    // warning.
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

    // Fails for missing and reserved names.
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

    /// Empty (3 x 0) if there are no triangles.
    [[nodiscard]] auto triangle_indices() const -> element_indices_view<3> {
        return topology_view<3>(face_vertices_name);
    }
    /// Empty (2 x 0) if there are no edges.
    [[nodiscard]] auto edge_indices() const -> element_indices_view<2> {
        return topology_view<2>(edge_vertices_name);
    }
    [[nodiscard]] auto triangle_count() const -> std::size_t {
        return triangle_indices().extent(1);
    }
    [[nodiscard]] auto edge_count() const -> std::size_t {
        return edge_indices().extent(1);
    }
    [[nodiscard]] auto has_triangle_indices() const -> bool {
        return triangle_count() > 0;
    }
    [[nodiscard]] auto has_edge_indices() const -> bool {
        return edge_count() > 0;
    }

    // Remove all attributes and topology.
    auto clear() -> void;
    // Replace the contents with a deep copy of other's.  The copied
    // attributes are new instances: handles into other do not refer to
    // them, and changing one geometry does not affect the other.
    auto copy_from(const MeshGeometry &other) -> void;

    // ── Dirty tracking ──────────────────────────────────────────────

    [[nodiscard]] auto version() const -> uint64_t { return _version; }

  private:
    /// Owning storage behind element_indices_view<K> (column-major).
    template <std::size_t K>
    using element_indices = ::zipper::detail::DataArray_<
        uint32_t,
        ::zipper::extents<K, std::dynamic_extent>,
        /*LeftMajor=*/false>;

    /// indices as K x count element indices, from either orientation;
    /// nullopt if neither extent is K.
    template <std::size_t K, typename MDS>
    static auto to_element_indices(const MDS &indices)
        -> std::optional<element_indices<K>>;

    template <std::size_t K>
    [[nodiscard]] auto topology_view(std::string_view name) const
        -> element_indices_view<K>;

    // Checks the name and element count of a user attribute, then inserts.
    auto add_user_attribute(MeshAttribute attr) -> MeshAttributeHandle;
    auto insert_attribute(MeshAttribute attr) -> MeshAttributeHandle;
    auto derive_edges() -> void;
    template <std::size_t K>
    auto set_topology(std::string_view name,
                      AttributeDomain domain,
                      element_indices_view<K> indices) -> void;

    std::vector<std::shared_ptr<MeshAttribute>> _attributes;
    bool _explicit_edges = false;
    uint64_t _version = 0;
};

} // namespace balsa::scene_graph

#include "MeshGeometry.hxx"
