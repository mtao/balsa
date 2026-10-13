#pragma once
#include <cstdint>
#include <string_view>

#include "AbstractFeature.hpp"
#include "MeshAttribute.hpp"
#include "RoleBinding.hpp"
#include "balsa/visualization/vulkan/mesh_render_state.hpp"

namespace balsa::scene_graph {

class MeshGeometry;

// ── BasicShading ────────────────────────────────────────────────────
//
// Shading context for the built-in lit, colour-mapped mesh renderer:
// binds the position, normal and scalar-field roles to attributes of the
// MeshGeometry on the same Object, and holds the render state (layers,
// shading model, colour source, colormap, material).
//
// Role assignments are kept and re-resolved against the geometry whenever
// it changes: bindings follow attributes that are replaced in place and
// are dropped for attributes that are removed.  A role you have not set
// yourself binds "vertex_positions" / "vertex_normals" automatically.
//
// version() is bumped when role assignments change, so renderers re-read
// the bound data; MeshGeometry::version() covers changes to the data.

class BasicShading : public AbstractFeature {
  public:
    using RenderState = visualization::vulkan::MeshRenderState;

    BasicShading();

    /// The MeshGeometry on the same Object, or nullptr.
    [[nodiscard]] auto geometry() const -> const MeshGeometry *;

    // ── Role assignment ─────────────────────────────────────────────
    //
    // Bind an attribute of geometry() (by name, or by handle from its
    // attributes()) to a role.  Returns false (and leaves the role
    // unbound) if the attribute is missing, not owned by geometry(), or
    // incompatible: position/normal require a vertex attribute with
    // >= 1 / >= 2 components; scalar requires a vertex attribute.
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
    // Change the selected component of the current scalar binding.
    auto set_scalar_component(int component) -> bool;

    auto clear_position() -> void;
    auto clear_normal() -> void;
    auto clear_scalar() -> void;

    [[nodiscard]] auto position_binding() const -> const RoleBinding &;
    [[nodiscard]] auto normal_binding() const -> const RoleBinding &;
    [[nodiscard]] auto scalar_binding() const -> const RoleBinding &;

    [[nodiscard]] auto scalar_component() const -> int {
        return _scalar.component;
    }

    // ── Convenience queries ─────────────────────────────────────────

    [[nodiscard]] auto has_positions() const -> bool {
        return position_binding().is_bound();
    }
    [[nodiscard]] auto has_normals() const -> bool {
        return normal_binding().is_bound();
    }
    [[nodiscard]] auto has_scalar_field() const -> bool {
        return scalar_binding().is_bound();
    }
    [[nodiscard]] auto vertex_count() const -> std::size_t {
        return position_binding().size();
    }

    // ── Dirty tracking ──────────────────────────────────────────────

    [[nodiscard]] auto version() const -> uint64_t { return _version; }

    // ── Render state ────────────────────────────────────────────────

    auto render_state() -> RenderState & { return _render_state; }
    [[nodiscard]] auto render_state() const -> const RenderState & {
        return _render_state;
    }

  private:
    // What a role is assigned to, and its binding resolved against the
    // current geometry.
    struct Role {
        MeshAttributeHandle attribute;
        int component = -1;       ///< scalar role only; -1 = magnitude
        bool user_set = false;    ///< assigned or cleared explicitly
        RoleBinding binding;
    };
    enum class Kind { Position, Normal, Scalar };

    /// Attribute a role binds automatically until it is set explicitly.
    static auto default_attribute_name(Kind kind) -> std::string_view;
    /// Whether attr can serve the role; logs why not when warn is set.
    static auto compatible(Kind kind, const MeshAttribute &attr, int component,
                           bool warn) -> bool;

    auto assign(Role &role, Kind kind, const MeshAttributeHandle &attr,
                int component) -> bool;
    auto clear(Role &role) -> void;
    // Re-resolve bindings if the geometry changed since the last call.
    auto sync() const -> void;
    auto resolve(Role &role, Kind kind) const -> void;
    [[nodiscard]] auto find(std::string_view name) const
        -> MeshAttributeHandle;

    // Bindings are a cache of (assignment, geometry), refreshed by sync().
    mutable Role _position;
    mutable Role _normal;
    mutable Role _scalar;
    mutable const MeshGeometry *_synced_geometry = nullptr;
    mutable uint64_t _synced_geometry_version = 0;

    RenderState _render_state;
    uint64_t _version = 0;
};

} // namespace balsa::scene_graph
