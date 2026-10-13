#pragma once
#include <concepts>
#include <cstdint>
#include <expected>
#include <limits>
#include <memory>
#include <optional>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <variant>
#include <vector>

#include "AbstractFeature.hpp"
#include "balsa/visualization/vulkan/mesh_render_state.hpp"
#include "types.hpp"

#include <zipper/DataArray.hpp>
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
// A named attribute: count() elements of component_count() values each,
// stored in its own element type (see AttributeType).  Storage is a
// column-major zipper::DataArray with extents (component_count, count),
// i.e. one column per element, so memory is element-major
// (x0 y0 z0 x1 y1 z1 ...).  The array is the only source of the shape.
//
// Constructed from any rank-1 (scalar per element) or rank-2
// (components x elements) mdspan of arithmetic values, in any layout or
// accessor.  Values keep their type, widened to the nearest stored type
// (e.g. int16 -> int32, uint8 -> uint32).

enum class AttributeType : uint8_t {
    Float32,
    Float64,
    Int32,
    UInt32,
};

/// Why a typed view of an attribute or binding could not be made, with
/// what was actually there.
struct ViewError {
    enum class Reason : uint8_t {
        Unbound,             ///< role binding has no attribute
        WrongType,           ///< values are stored as actual_type
        WrongComponentCount, ///< values have actual_components components
    };
    Reason reason;
    AttributeType actual_type = AttributeType::Float32;
    uint8_t actual_components = 0;
};

namespace detail {
    /// Stored type for attribute values given as T; no `type` member when T
    /// is unsupported.
    template <typename T>
    struct attribute_storage {};
    template <>
    struct attribute_storage<float> {
        using type = float;
    };
    template <>
    struct attribute_storage<double> {
        using type = double;
    };
    template <std::signed_integral T>
        requires(sizeof(T) <= sizeof(int32_t))
    struct attribute_storage<T> {
        using type = int32_t;
    };
    template <std::unsigned_integral T>
        requires(sizeof(T) <= sizeof(uint32_t) && !std::same_as<T, bool>)
    struct attribute_storage<T> {
        using type = uint32_t;
    };

    template <typename T>
    using attribute_storage_t = typename attribute_storage<T>::type;
} // namespace detail

/// Value types MeshAttribute accepts: float, double, or an integer of at
/// most 32 bits (stored as float, double, int32 or uint32).
template <typename T>
concept AttributeValue =
    requires { typename detail::attribute_storage<std::remove_cv_t<T>>::type; };

class MeshAttribute {
  public:
    /// Plain column-major (components x count) storage for one attribute;
    /// no arithmetic semantics.  Wrap a view in zipper::MatrixBase for math.
    /// (zipper's "left-major" means leftmost index slowest, i.e. row-major;
    /// LeftMajor = false is column-major.  The public DataArray alias only
    /// covers static extents.)
    template <typename T>
    using storage_type = ::zipper::detail::
        DataArray_<T, ::zipper::dextents<2>, /*LeftMajor=*/false>;
    template <typename T, std::size_t D = std::dynamic_extent>
    using const_view_type =
        ::zipper::mdspan<const T,
                         ::zipper::extents<D, std::dynamic_extent>,
                         ::zipper::storage::col_major>;

    /// Largest component count representable by component_count().
    static constexpr std::size_t max_components =
        std::numeric_limits<uint8_t>::max();

    MeshAttribute() = default;
    /// Precondition: data.extent(0) <= max_components for rank-2 data.
    template <AttributeValue T,
              typename Extents,
              typename Layout,
              typename Accessor>
        requires(Extents::rank() == 1 || Extents::rank() == 2)
    MeshAttribute(std::string name,
                  AttributeDomain domain,
                  ::zipper::mdspan<T, Extents, Layout, Accessor> data);

    std::string name;
    AttributeDomain domain = AttributeDomain::Vertex;

    [[nodiscard]] auto type() const -> AttributeType {
        return static_cast<AttributeType>(m_values.index());
    }
    template <typename T>
    [[nodiscard]] auto holds() const -> bool {
        return std::holds_alternative<storage_type<T>>(m_values);
    }

    /// Calls f with a dynamic const view of the values, whatever their type.
    template <typename F>
    auto visit(F &&f) const -> decltype(auto) {
        return std::visit(
            [&](const auto &m) -> decltype(auto) {
                return f(m.expression().as_mdspan());
            },
            m_values);
    }

    [[nodiscard]] auto component_count() const -> uint8_t {
        return static_cast<uint8_t>(
            visit([](const auto &v) { return v.extent(0); }));
    }
    [[nodiscard]] auto count() const -> std::size_t {
        return visit([](const auto &v) { return v.extent(1); });
    }

    /// Extents (component_count(), count()).  Fails with WrongType unless
    /// the values are stored as T, and, for static D, with
    /// WrongComponentCount unless component_count() == D.
    template <typename T, std::size_t D = std::dynamic_extent>
    [[nodiscard]] auto view() const
        -> std::expected<const_view_type<T, D>, ViewError> {
        const auto *m = std::get_if<storage_type<T>>(&m_values);
        if (m == nullptr) {
            return std::unexpected(ViewError{ ViewError::Reason::WrongType,
                                              type(),
                                              component_count() });
        }
        const auto all = m->expression().as_mdspan();
        if constexpr (D == std::dynamic_extent) {
            return all;
        } else {
            if (all.extent(0) != D) {
                return std::unexpected(
                    ViewError{ ViewError::Reason::WrongComponentCount,
                               type(),
                               component_count() });
            }
            return const_view_type<T, D>(all.data_handle(), all.extent(1));
        }
    }

  private:
    // Alternative order matches AttributeType.
    std::variant<storage_type<float>,
                 storage_type<double>,
                 storage_type<int32_t>,
                 storage_type<uint32_t>>
        m_values;
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
// attribute via a MeshAttributeHandle, so reads need no name lookup.
// Values keep the attribute's type; converting to what a renderer needs
// (e.g. float vertex data) is the renderer's job.
//
// A binding views the whole attribute, one selected component (a strided
// view into the attribute, no copy), or the per-element magnitude
// (computed into a derived attribute: float stays float, other types
// become double).

struct RoleBinding {
    /// (components, size) view of the bound values.  Strided: a selected
    /// component steps over the other components of each element.
    template <typename T, std::size_t D = std::dynamic_extent>
    using view_type = ::zipper::mdspan<const T,
                                       ::zipper::extents<D, std::dynamic_extent>,
                                       ::zipper::storage::layout_stride>;

    [[nodiscard]] auto is_bound() const -> bool { return _attr.valid(); }
    [[nodiscard]] auto attribute() const -> const MeshAttributeHandle & {
        return _attr;
    }
    /// Components per element in the bound values (1 for a selected
    /// component or the magnitude).
    [[nodiscard]] auto component_count() const -> uint8_t {
        if (!is_bound()) return 0;
        return _component ? 1 : source().component_count();
    }
    [[nodiscard]] auto size() const -> std::size_t {
        return !_attr ? 0 : _attr->count();
    }
    /// Type of the bound values (meaningless when unbound).
    [[nodiscard]] auto type() const -> AttributeType {
        return !_attr ? AttributeType::Float32 : source().type();
    }
    auto clear() -> void;

    /// Fails with Unbound, WrongType unless the values are T, or (for
    /// static D) WrongComponentCount unless component_count() == D.
    template <typename T, std::size_t D = std::dynamic_extent>
    [[nodiscard]] auto view() const
        -> std::expected<view_type<T, D>, ViewError>;

    /// Calls f with view<T>() for the bound values' type T.  Precondition:
    /// is_bound().
    template <typename F>
    auto visit(F &&f) const -> decltype(auto);

  private:
    friend class MeshData;
    [[nodiscard]] auto source() const -> const MeshAttribute & {
        return _magnitude ? *_magnitude : *_attr;
    }

    MeshAttributeHandle _attr;
    std::optional<uint8_t> _component; ///< selected component of source()
    std::shared_ptr<const MeshAttribute> _magnitude;
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
