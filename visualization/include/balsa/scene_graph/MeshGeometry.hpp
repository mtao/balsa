#pragma once
#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "AttributeSemantic.hpp"
#include "MeshAttribute.hpp"

#include <zipper/DataArray.hpp>
#include <zipper/storage/layout_types.hpp>
#include <zipper/types.hpp>

namespace balsa::scene_graph {

// MeshGeometry::add_attribute options.
struct AttributeOptions {
    // Tag to apply (see set_semantic).
    std::optional<AttributeSemantic> semantic = {};
    // See MeshGeometry::SemanticEntry::set.
    uint8_t set = 0;
    // 8/16-bit integers only: values map to [0, 1] / [-1, 1].
    bool normalized = false;
};

// ── MeshGeometry ────────────────────────────────────────────────────
//
// A mesh's data as named, typed attributes on vertices, edges and faces.
// It is a resource, not part of any one Object: Objects show it through a
// MeshInstance feature (usually via std::shared_ptr, so several Objects
// can share one geometry), and shading features (e.g. BasicShading)
// choose which attributes they use.  It knows nothing about rendering.
//
// Topology: d-simplices (d = 1..3) stored as reserved '.'-prefixed
// attributes in AttributeDomain(d), d + 1 indices each, set only through
// set_simplices.  Only what is set is stored; drawable forms (boundaries,
// edges) come from GeometryViews.
//
// Attribute meaning is a separate (semantic, set) -> attribute manifest;
// names are labels only.
//
// version() is bumped on every change, so consumers know when to re-read.

class MeshGeometry {
  public:
    /// K x count vertex indices (K = d + 1), element-major in memory.
    template <std::size_t K>
    using element_indices_view = MeshAttribute::const_view_type<uint32_t, K>;

    /// Highest supported simplex dimension.
    static constexpr std::size_t max_dimension = 3;
    /// Topology attribute names, by simplex dimension.
    static constexpr std::string_view edge_vertices_name = ".edge_vertices";
    static constexpr std::string_view face_vertices_name = ".face_vertices";
    static constexpr std::string_view cell_vertices_name = ".cell_vertices";
    static constexpr std::array<std::string_view, max_dimension + 1>
        simplex_names{{"", edge_vertices_name, face_vertices_name,
                       cell_vertices_name}};
    /// Names starting with '.' are reserved for MeshGeometry's own
    /// attributes (the topology).
    [[nodiscard]] static constexpr auto is_reserved_name(std::string_view name)
        -> bool {
        return name.starts_with('.');
    }

    MeshGeometry();
    // Copying would share attribute instances; use clone() for a deep
    // copy.
    MeshGeometry(const MeshGeometry &) = delete;
    auto operator=(const MeshGeometry &) -> MeshGeometry & = delete;
    MeshGeometry(MeshGeometry &&) noexcept = default;
    // Replacing the contents is a change: version() moves past both.
    auto operator=(MeshGeometry &&other) noexcept -> MeshGeometry &;

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
    // warning, as is an inapplicable options.semantic (the attribute is
    // still added).
    template <AttributeValue T,
              typename Extents,
              typename Layout,
              typename Accessor>
        requires(Extents::rank() >= 1 && Extents::rank() <= 3)
    auto add_attribute(std::string name,
                       AttributeDomain domain,
                       ::zipper::mdspan<T, Extents, Layout, Accessor> data,
                       AttributeOptions options = {}) -> MeshAttributeHandle;
    // Convenience: flat element-major data (any contiguous range),
    // component_count values per element.
    template <std::ranges::contiguous_range R>
        requires std::ranges::sized_range<R>
    auto add_attribute(std::string name,
                       AttributeDomain domain,
                       uint8_t component_count,
                       const R &data,
                       AttributeOptions options = {}) -> MeshAttributeHandle;

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

    // ── Semantics (the manifest) ───────────────────────────────────
    //
    // Each (semantic, set) names at most one attribute; an attribute may
    // have several.  Entries are dropped with their attribute, or when it is
    // replaced by values the semantic rejects.

    struct SemanticEntry {
        AttributeSemantic semantic;
        // Index among same-semantic attributes (glTF's TEXCOORD_n);
        // meaningless by itself.
        uint8_t set = 0;
        MeshAttributeHandle attribute;
    };

    // Fails if `attribute` is not ours or the semantic rejects it; replaces
    // any previous attribute for (semantic, set).
    auto set_semantic(AttributeSemantic semantic,
                      const MeshAttributeHandle &attribute,
                      uint8_t set = 0) -> bool;
    auto clear_semantic(AttributeSemantic semantic, uint8_t set = 0) -> bool;
    // Invalid handle if nothing carries (semantic, set).
    [[nodiscard]] auto attribute_for(AttributeSemantic semantic,
                                     uint8_t set = 0) const
        -> MeshAttributeHandle;
    [[nodiscard]] auto semantics() const -> std::span<const SemanticEntry> {
        return _semantics;
    }

    // ── Topology ────────────────────────────────────────────────────

    // Any rank-2 integral mdspan, N x (D + 1) or (D + 1) x N (rows win if
    // both fit), any layout.  False, changing nothing, if neither fits.
    template <std::size_t D,
              typename T,
              typename Extents,
              typename Layout,
              typename Accessor>
        requires(D >= 1 && D <= max_dimension && Extents::rank() == 2)
    auto set_simplices(::zipper::mdspan<T, Extents, Layout, Accessor> indices)
        -> bool {
        const auto converted = to_element_indices<D + 1>(indices);
        if (!converted) return false;
        set_topology<D + 1>(simplex_names[D],
                            static_cast<AttributeDomain>(D),
                            converted->expression().as_mdspan());
        return true;
    }

    /// (D + 1) x count.
    template <std::size_t D>
        requires(D >= 1 && D <= max_dimension)
    [[nodiscard]] auto simplices() const -> element_indices_view<D + 1> {
        return topology_view<D + 1>(simplex_names[D]);
    }
    /// 0 for d outside 1..max_dimension.
    [[nodiscard]] auto simplex_count(std::size_t d) const -> std::size_t;
    [[nodiscard]] auto has_simplices(std::size_t d) const -> bool {
        return simplex_count(d) > 0;
    }
    /// Highest dimension with simplices; 0 without topology.
    [[nodiscard]] auto dimension() const -> std::size_t;

    // Aliases for dimensions 2 and 1.
    template <typename T, typename Extents, typename Layout, typename Accessor>
        requires(Extents::rank() == 2)
    auto set_triangle_indices(
        ::zipper::mdspan<T, Extents, Layout, Accessor> indices) -> bool {
        return set_simplices<2>(indices);
    }
    template <typename T, typename Extents, typename Layout, typename Accessor>
        requires(Extents::rank() == 2)
    auto set_edge_indices(::zipper::mdspan<T, Extents, Layout, Accessor> indices)
        -> bool {
        return set_simplices<1>(indices);
    }
    [[nodiscard]] auto triangle_indices() const -> element_indices_view<3> {
        return simplices<2>();
    }
    [[nodiscard]] auto edge_indices() const -> element_indices_view<2> {
        return simplices<1>();
    }
    [[nodiscard]] auto triangle_count() const -> std::size_t {
        return simplex_count(2);
    }
    [[nodiscard]] auto edge_count() const -> std::size_t {
        return simplex_count(1);
    }
    [[nodiscard]] auto has_triangle_indices() const -> bool {
        return has_simplices(2);
    }
    [[nodiscard]] auto has_edge_indices() const -> bool {
        return has_simplices(1);
    }

    // Remove all attributes and topology.
    auto clear() -> void;
    // A deep copy.  Its attributes are new instances: handles into this
    // geometry do not refer to them, and changing one geometry does not
    // affect the other.
    [[nodiscard]] auto clone() const -> MeshGeometry;
    // Topology-free geometry sharing this one's vertex attribute instances
    // and their semantics, for views.  Adding attributes under the shared
    // names would overwrite them here too.
    [[nodiscard]] auto share_vertex_attributes() const -> MeshGeometry;

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

    // Checks the name and element count of a user attribute, inserts it
    // and applies options.semantic.
    auto add_user_attribute(MeshAttribute attr, const AttributeOptions &options)
        -> MeshAttributeHandle;
    auto insert_attribute(MeshAttribute attr) -> MeshAttributeHandle;
    template <std::size_t K>
    auto set_topology(std::string_view name,
                      AttributeDomain domain,
                      element_indices_view<K> indices) -> void;

    std::vector<std::shared_ptr<MeshAttribute>> _attributes;
    std::vector<SemanticEntry> _semantics;
    uint64_t _version = 0;
};

} // namespace balsa::scene_graph

#include "MeshGeometry.hxx"
