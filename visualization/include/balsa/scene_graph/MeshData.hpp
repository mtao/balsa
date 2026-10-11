#if !defined(BALSA_SCENE_GRAPH_MESH_DATA_HPP)
#define BALSA_SCENE_GRAPH_MESH_DATA_HPP

#include <cstdint>
#include <memory>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "AbstractFeature.hpp"
#include "balsa/visualization/vulkan/mesh_render_state.hpp"
#include "types.hpp"

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
// A named, flat attribute buffer: count() elements of component_count
// floats each, stored contiguously.  Input data of any floating-point
// type is converted to float on insertion so it is GPU-ready.

struct MeshAttribute {
    std::string name;
    AttributeDomain domain = AttributeDomain::Vertex;
    uint8_t component_count = 0;
    std::vector<float> data;

    auto count() const -> std::size_t {
        return component_count == 0 ? 0 : data.size() / component_count;
    }
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
    auto is_bound() const -> bool { return _attr.valid(); }
    auto attribute() const -> const MeshAttributeHandle & { return _attr; }
    // Number of float components per element in raw_data().
    auto component_count() const -> uint8_t {
        return !_attr ? 0 : _extracts ? 1 : _attr->component_count;
    }
    auto size() const -> std::size_t { return !_attr ? 0 : _attr->count(); }
    auto raw_data() const -> const void * {
        if (!_attr) return nullptr;
        return _extracts ? _extracted.data() : _attr->data.data();
    }
    auto clear() -> void;

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
    // Add (or replace, if the name exists) an attribute.  data holds
    // count * component_count values, element-major.

    auto add_attribute(std::string name,
                       AttributeDomain domain,
                       uint8_t component_count,
                       std::span<const float> data) -> MeshAttributeHandle;
    auto add_attribute(std::string name,
                       AttributeDomain domain,
                       uint8_t component_count,
                       std::span<const double> data) -> MeshAttributeHandle;

    auto remove_attribute(std::string_view name) -> bool;

    // Random-access view of MeshAttributeHandle.
    auto attributes() const {
        return _attributes
               | std::views::transform(
                   [](const std::shared_ptr<MeshAttribute> &a) {
                       return MeshAttributeHandle(a);
                   });
    }
    // Invalid handle if not found.
    auto find_attribute(std::string_view name) const -> MeshAttributeHandle;

    // ── Topology ────────────────────────────────────────────────────

    auto set_triangle_indices(std::span<const uint32_t> indices) -> void;
    // Explicit edges override edges derived from triangles.
    auto set_edge_indices(std::span<const uint32_t> indices) -> void;

    auto triangle_indices() const -> std::span<const uint32_t> {
        return _tri_indices;
    }
    auto edge_indices() const -> std::span<const uint32_t> {
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

    auto position_binding() const -> const RoleBinding & { return _position; }
    auto normal_binding() const -> const RoleBinding & { return _normal; }
    auto scalar_binding() const -> const RoleBinding & { return _scalar; }

    auto scalar_component() const -> int { return _scalar_component; }

    // ── Convenience queries ─────────────────────────────────────────

    auto has_positions() const -> bool { return _position.is_bound(); }
    auto has_normals() const -> bool { return _normal.is_bound(); }
    auto has_scalar_field() const -> bool { return _scalar.is_bound(); }
    auto has_triangle_indices() const -> bool { return !_tri_indices.empty(); }
    auto has_edge_indices() const -> bool { return !_edge_indices.empty(); }

    auto vertex_count() const -> std::size_t { return _position.size(); }
    auto triangle_count() const -> std::size_t {
        return _tri_indices.size() / 3;
    }
    auto edge_count() const -> std::size_t { return _edge_indices.size() / 2; }

    // ── Dirty tracking ──────────────────────────────────────────────

    auto version() const -> uint64_t { return _version; }

    // ── Render state ────────────────────────────────────────────────

    auto render_state() -> RenderState & { return _render_state; }
    auto render_state() const -> const RenderState & { return _render_state; }

  private:
    auto insert_attribute(MeshAttribute attr) -> MeshAttributeHandle;
    auto owns(const MeshAttributeHandle &attr) const -> bool;
    // Re-validate bindings that reference attr after it changed in place.
    auto refresh_bindings(const MeshAttributeHandle &attr) -> void;
    auto derive_edges() -> void;

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

#endif
