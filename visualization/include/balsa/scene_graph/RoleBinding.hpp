#pragma once
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>

#include "MeshAttribute.hpp"

#include <zipper/storage/layout_types.hpp>
#include <zipper/types.hpp>

namespace balsa::scene_graph {

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

    /// The whole attribute.
    static auto whole(MeshAttributeHandle attr) -> RoleBinding;
    /// One component of attr.  Precondition: c < attr->component_count().
    static auto component(MeshAttributeHandle attr, uint8_t c) -> RoleBinding;
    /// Per-element magnitudes of attr.
    static auto magnitude(MeshAttributeHandle attr) -> RoleBinding;

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
    [[nodiscard]] auto source() const -> const MeshAttribute & {
        return _magnitude ? *_magnitude : *_attr;
    }

    MeshAttributeHandle _attr;
    std::optional<uint8_t> _component; ///< selected component of source()
    std::shared_ptr<const MeshAttribute> _magnitude;
};

} // namespace balsa::scene_graph

#include "RoleBinding.hxx"
