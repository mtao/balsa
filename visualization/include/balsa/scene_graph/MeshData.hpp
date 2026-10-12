#pragma once
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "AbstractFeature.hpp"
#include "balsa/visualization/vulkan/mesh_render_state.hpp"
#include "types.hpp"

#include <zipper/storage/layout_types.hpp>
#include <zipper/types.hpp>

namespace balsa::scene_graph {

// ── AttributeDomain ─────────────────────────────────────────────────
//
// Which mesh element an attribute is defined on.  The values match
// simplex dimension so UI code can display them uniformly.

enum class AttributeDomain : int8_t {
    Vertex = 0,
    Edge = 1,
    Face = 2,
};

// ── MeshAttribute ───────────────────────────────────────────────────
//
// A named attribute: count() elements of component_count() floats
// each.  Storage is column-major with extents (component_count, count),
// i.e. one column per element, so memory is element-major
// (x0 y0 z0 x1 y1 z1 ...) and can be uploaded to the GPU directly.
//
// Constructed from any rank-1 (scalar per element) or rank-2
// (components x elements) mdspan of arithmetic values, in any layout;
// the shape is taken from the mdspan and values are converted to float.

class MeshAttribute {
  public:
    template <std::size_t D = std::dynamic_extent>
    using view_type = ::zipper::mdspan<float,
                                       ::zipper::extents<D, std::dynamic_extent>,
                                       ::zipper::storage::col_major>;
    template <std::size_t D = std::dynamic_extent>
    using const_view_type =
        ::zipper::mdspan<const float,
                         ::zipper::extents<D, std::dynamic_extent>,
                         ::zipper::storage::col_major>;

    /// Largest component count representable by component_count().
    static constexpr std::size_t max_components =
        std::numeric_limits<uint8_t>::max();

    MeshAttribute() = default;
    /// Precondition: data.extent(0) <= max_components for rank-2 data.
    template <typename T, typename Extents, typename Layout, typename Accessor>
        requires(Extents::rank() == 1 || Extents::rank() == 2)
    MeshAttribute(std::string name,
                  AttributeDomain domain,
                  ::zipper::mdspan<T, Extents, Layout, Accessor> data);

    std::string name;
    AttributeDomain domain = AttributeDomain::Vertex;

    [[nodiscard]] auto component_count() const -> uint8_t {
        return m_component_count;
    }
    [[nodiscard]] auto count() const -> std::size_t { return m_count; }

    /// Flat element-major storage.
    [[nodiscard]] auto data() const -> std::span<const float> {
        return m_data;
    }

    /// Extents (component_count(), count()).
    [[nodiscard]] auto view() const -> const_view_type<> {
        return const_view_type<>(m_data.data(),
                                 std::size_t{m_component_count},
                                 m_count);
    }
    [[nodiscard]] auto view() -> view_type<> {
        return view_type<>(m_data.data(), std::size_t{m_component_count},
                           m_count);
    }
    /// Extents (D, count()); nullopt if component_count() != D.
    template <std::size_t D>
    [[nodiscard]] auto view() const -> std::optional<const_view_type<D>> {
        if (m_component_count != D) return std::nullopt;
        return const_view_type<D>(m_data.data(), m_count);
    }
    template <std::size_t D>
    [[nodiscard]] auto view() -> std::optional<view_type<D>> {
        if (m_component_count != D) return std::nullopt;
        return view_type<D>(m_data.data(), m_count);
    }

  private:
    uint8_t m_component_count = 0;
    std::size_t m_count = 0;
    std::vector<float> m_data;
};

// ── MeshAttributeHandle ─────────────────────────────────────────────
//
// Cheap, copyable reference to an attribute owned by a MeshData.
// Compares by identity.  Keeps the attribute's data alive even if it
// is later removed from the MeshData (it is then simply detached).

class MeshAttributeHandle {
  public:
    MeshAttributeHandle() = default;

    auto valid() const -> bool { return _attr != nullptr; }
    explicit operator bool() const { return valid(); }
    auto operator*() const -> const MeshAttribute & { return *_attr; }
    auto operator->() const -> const MeshAttribute * { return _attr.get(); }
    auto operator==(const MeshAttributeHandle &o) const -> bool {
        return _attr == o._attr;
    }

  private:
    friend class MeshData;
    explicit MeshAttributeHandle(std::shared_ptr<const MeshAttribute> a)
      : _attr(std::move(a)) {}
    std::shared_ptr<const MeshAttribute> _attr;
};

// ── RoleBinding ─────────────────────────────────────────────────────
//
// Binds a visualization role (positions, normals, scalar field) to an
// attribute via a MeshAttributeHandle, so raw_data()/size() are direct
// reads with no name lookup.  For scalar component/magnitude
// extraction, raw_data() points into an owned extracted buffer.

struct RoleBinding {
    [[nodiscard]] auto is_bound() const -> bool { return _attr.valid(); }
    [[nodiscard]] auto attribute() const -> const MeshAttributeHandle & {
        return _attr;
    }
    // Number of float components per element in raw_data().
    [[nodiscard]] auto component_count() const -> uint8_t {
        return !_attr ? 0 : _extracts ? 1 : _attr->component_count();
    }
    [[nodiscard]] auto size() const -> std::size_t {
        return !_attr ? 0 : _attr->count();
    }
    [[nodiscard]] auto raw_data() const -> const void * {
        if (!_attr) return nullptr;
        return _extracts ? _extracted.data() : _attr->data().data();
    }
    auto clear() -> void;

    /// Plain mdspan views over raw_data() with extents
    /// (component_count(), size()), column-major:
    /// (c, i) -> raw_data()[c + component_count() * i].
    /// Wrap in zipper types if desired.
    template <std::size_t D = std::dynamic_extent>
    using view_type = MeshAttribute::const_view_type<D>;

    /// Empty (0 x 0) if unbound.
    [[nodiscard]] auto view() const -> view_type<> {
        return view_type<>(static_cast<const float *>(raw_data()),
                           std::size_t{component_count()},
                           size());
    }
    /// nullopt if unbound or component_count() != D.
    template <std::size_t D>
    [[nodiscard]] auto view() const -> std::optional<view_type<D>> {
        if (!is_bound() || component_count() != D) return std::nullopt;
        return view_type<D>(static_cast<const float *>(raw_data()), size());
    }

  private:
    friend class MeshData;
    MeshAttributeHandle _attr;
    bool _extracts = false; // raw_data() reads _extracted
    std::vector<float> _extracted;
};

// ── MeshData ────────────────────────────────────────────────────────
//
// Feature holding CPU-side mesh geometry and appearance parameters as
// plain buffers: named attributes, triangle/edge index buffers, and
// role bindings selecting which attributes drive rendering.
//
// Attributes named "vertex_positions" / "vertex_normals" are bound to
// the position / normal roles automatically when added.  Edges are
// derived from triangles unless set explicitly.
//
// The version counter is bumped on any change so the rendering layer
// knows when GPU buffers need re-uploading.

class MeshData : public AbstractFeature {
  public:
    using RenderState = visualization::vulkan::MeshRenderState;

    MeshData();

    // ── Attributes ──────────────────────────────────────────────────
    //
    // Add (or replace, if the name exists) an attribute.  data is a
    // rank-1 mdspan (one scalar per element) or a rank-2 mdspan with
    // extents (components, elements) in any layout; e.g. N x 3
    // row-major positions are a 3 x N layout_left mdspan over the same
    // memory.  Returns an invalid handle if the shape is unusable.
    template <typename T, typename Extents, typename Layout, typename Accessor>
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

    // Remove all attributes, bindings and topology.
    auto clear() -> void;

    // ── Role assignment ─────────────────────────────────────────────
    //
    // Bind an attribute (by name, or by handle from attributes()) to a
    // role.  Returns false (and leaves the role unbound) if the attribute
    // is missing, not owned by this MeshData, or incompatible:
    // position/normal require a vertex attribute with >= 1 / >= 2
    // components; scalar requires a vertex attribute.
    //
    // For the scalar role, component selects which element of a
    // multi-component attribute to use: -1 = magnitude.

    auto assign_position(std::string_view name) -> bool;
    auto assign_normal(std::string_view name) -> bool;
    auto assign_scalar(std::string_view name, int component = -1) -> bool;
    auto assign_position(const MeshAttributeHandle &attr) -> bool;
    auto assign_normal(const MeshAttributeHandle &attr) -> bool;
    auto assign_scalar(const MeshAttributeHandle &attr, int component = -1)
        -> bool;
    // Change the extracted component of the current scalar binding.
    auto set_scalar_component(int component) -> bool;

    auto clear_position() -> void;
    auto clear_normal() -> void;
    auto clear_scalar() -> void;

    [[nodiscard]] auto position_binding() const -> const RoleBinding & {
        return _position;
    }
    [[nodiscard]] auto normal_binding() const -> const RoleBinding & {
        return _normal;
    }
    [[nodiscard]] auto scalar_binding() const -> const RoleBinding & {
        return _scalar;
    }

    [[nodiscard]] auto scalar_component() const -> int {
        return _scalar_component;
    }

    // ── Convenience queries ─────────────────────────────────────────

    [[nodiscard]] auto has_positions() const -> bool {
        return _position.is_bound();
    }
    [[nodiscard]] auto has_normals() const -> bool {
        return _normal.is_bound();
    }
    [[nodiscard]] auto has_scalar_field() const -> bool {
        return _scalar.is_bound();
    }
    [[nodiscard]] auto has_triangle_indices() const -> bool {
        return !_tri_indices.empty();
    }
    [[nodiscard]] auto has_edge_indices() const -> bool {
        return !_edge_indices.empty();
    }

    [[nodiscard]] auto vertex_count() const -> std::size_t {
        return _position.size();
    }
    [[nodiscard]] auto triangle_count() const -> std::size_t {
        return _tri_indices.size() / 3;
    }
    [[nodiscard]] auto edge_count() const -> std::size_t {
        return _edge_indices.size() / 2;
    }

    // ── Dirty tracking ──────────────────────────────────────────────

    [[nodiscard]] auto version() const -> uint64_t { return _version; }

    // ── Render state ────────────────────────────────────────────────

    auto render_state() -> RenderState & { return _render_state; }
    [[nodiscard]] auto render_state() const -> const RenderState & {
        return _render_state;
    }

  private:
    auto insert_attribute(MeshAttribute attr) -> MeshAttributeHandle;
    [[nodiscard]] auto owns(const MeshAttributeHandle &attr) const -> bool;
    // Re-validate bindings that reference attr after it changed in place.
    auto refresh_bindings(const MeshAttributeHandle &attr) -> void;
    auto derive_edges() -> void;
    auto assign_triangles(std::vector<uint32_t> flat) -> void;
    auto assign_edges(std::vector<uint32_t> flat) -> void;

    static auto bind_vector(const MeshAttributeHandle &attr,
                            uint8_t min_components) -> RoleBinding;
    static auto bind_scalar(const MeshAttributeHandle &attr, int component)
        -> RoleBinding;

    std::vector<std::shared_ptr<MeshAttribute>> _attributes;

    RoleBinding _position;
    RoleBinding _normal;
    RoleBinding _scalar;
    int _scalar_component = -1;

    std::vector<uint32_t> _tri_indices;
    std::vector<uint32_t> _edge_indices;
    bool _explicit_edges = false;

    RenderState _render_state;
    uint64_t _version = 0;
};

} // namespace balsa::scene_graph

#include "MeshData.hxx"
