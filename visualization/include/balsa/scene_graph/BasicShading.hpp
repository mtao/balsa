#pragma once
#include <cstdint>
#include <optional>
#include <string_view>

#include "AbstractFeature.hpp"
#include "MeshAttribute.hpp"
#include "MeshRenderer.hpp"

#include <vector>

namespace balsa::scene_graph {

class MeshGeometry;

// ── BasicShading ────────────────────────────────────────────────────
//
// Shading context for the MeshGeometry shown by the Object's
// MeshInstance: an ordered list of render layers, each pairing a
// MeshRenderer with raw bindings of the renderer's semantic roles to
// attributes of that geometry.
//
// A layer stores only explicit bindings.  layer_attribute() resolves a
// role against the current geometry: an explicit binding follows its
// attribute by name (so bindings survive the geometry being replaced
// or cloned, and drop out when the attribute is removed); a role
// without one falls back to the renderer's default attribute.  Either
// way the attribute must satisfy the renderer's requirement.
//
// version() is bumped when bindings change, so renderers re-read the
// bound data; MeshGeometry::version() covers changes to the data.

class BasicShading : public AbstractFeature {
  public:
    BasicShading();

    /// The geometry shown by the Object's MeshInstance, or nullptr.
    [[nodiscard]] auto geometry() const -> const MeshGeometry *;

    // ── Copying ─────────────────────────────────────────────────────

    // Copy other's render layers, including their explicit bindings,
    // which re-target to same-named attributes of this geometry.
    auto copy_from(const BasicShading &other) -> void;

    // ── Dirty tracking ──────────────────────────────────────────────

    [[nodiscard]] auto version() const -> uint64_t { return _version; }

    // ── Render layers ───────────────────────────────────────────────

    auto render_layers() -> std::vector<MeshRenderLayer> & {
        return _render_layers;
    }
    [[nodiscard]] auto render_layers() const
        -> const std::vector<MeshRenderLayer> & {
        return _render_layers;
    }
    [[nodiscard]] auto find_layer(MeshLayerId id) -> MeshRenderLayer *;
    [[nodiscard]] auto find_layer(MeshLayerId id) const -> const MeshRenderLayer *;
    [[nodiscard]] auto find_layer(std::string_view renderer_id) -> MeshRenderLayer *;
    [[nodiscard]] auto find_layer(std::string_view renderer_id) const
        -> const MeshRenderLayer *;

    // ── Role bindings ───────────────────────────────────────────────
    //
    // Bind role of a layer's renderer to an attribute of geometry().
    // Fails (changing nothing) if the layer or role does not exist, the
    // attribute is not owned by geometry(), or it does not satisfy the
    // renderer's requirement.
    auto assign_layer_role(MeshLayerId layer_id, std::string_view role,
                           const MeshAttributeHandle &attribute) -> bool;
    auto assign_layer_role(MeshLayerId layer_id, std::string_view role,
                           std::string_view attribute_name) -> bool;
    // Explicitly unbind role (no default fallback).
    auto clear_layer_role(MeshLayerId layer_id, std::string_view role) -> bool;
    // Drop the explicit binding so role falls back to its default.
    auto reset_layer_role(MeshLayerId layer_id, std::string_view role) -> bool;

    /// The attribute role resolves to for layer, or an invalid handle.
    [[nodiscard]] auto layer_attribute(const MeshRenderLayer &layer,
                                       std::string_view role) const
        -> MeshAttributeHandle;

  private:
    auto set_binding(MeshLayerId layer_id, std::string_view role,
                     std::optional<MeshRoleBinding> binding) -> bool;

    std::vector<MeshRenderLayer> _render_layers;
    uint64_t _next_layer_id = 1;
    uint64_t _version = 0;
};

} // namespace balsa::scene_graph
