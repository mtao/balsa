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
// role against the layer's view geometry: an explicit binding follows its
// attribute by name (surviving replacement or cloning); otherwise the
// role's semantic is used.  Either way the role's requirement must hold.
//
// version() is bumped when bindings change, so renderers re-read the
// bound data; MeshGeometry::version() covers changes to the data.

class BasicShading : public AbstractFeature {
  public:
    // Default layers for a triangle mesh.
    BasicShading();
    // Assigns fresh layer ids.
    explicit BasicShading(std::vector<MeshRenderLayer> layers);

    /// The geometry shown by the Object's MeshInstance, or nullptr.
    [[nodiscard]] auto geometry() const -> const MeshGeometry *;
    /// The layer's view of geometry(); bindings, candidates and fitting
    /// refer to it.
    [[nodiscard]] auto layer_geometry(const MeshRenderLayer &layer) const
        -> std::shared_ptr<const MeshGeometry>;

    // ── Copying ─────────────────────────────────────────────────────

    // Copy other's render layers, including their explicit bindings,
    // which re-target to same-named attributes of this geometry.
    auto copy_from(const BasicShading &other) -> void;

    // Runs MeshRenderer::fit_to per layer, disabling (never enabling)
    // layers with nothing to draw.
    auto fit_to_geometry() -> void;

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
    // Binds a role to an attribute of the layer's geometry.  Fails, changing
    // nothing, for unknown layers or roles, foreign attributes, or unmet
    // requirements.
    auto assign_layer_role(MeshLayerId layer_id, MeshRole role,
                           const MeshAttributeHandle &attribute) -> bool;
    auto assign_layer_role(MeshLayerId layer_id, MeshRole role,
                           std::string_view attribute_name) -> bool;
    // Explicitly unbind role (no default fallback).
    auto clear_layer_role(MeshLayerId layer_id, MeshRole role) -> bool;
    // Drop the explicit binding so role falls back to its default.
    auto reset_layer_role(MeshLayerId layer_id, MeshRole role) -> bool;

    /// The attribute role resolves to for layer, or an invalid handle.
    [[nodiscard]] auto layer_attribute(const MeshRenderLayer &layer,
                                       MeshRole role) const
        -> MeshAttributeHandle;

    /// Bindable attributes for role: semantic-tagged ones (by set), then
    /// others the role accepts; never topology.
    [[nodiscard]] auto candidates(const MeshRenderLayer &layer,
                                  MeshRole role) const
        -> std::vector<MeshAttributeHandle>;

  private:
    auto set_binding(MeshLayerId layer_id, MeshRole role,
                     std::optional<MeshRoleBinding> binding) -> bool;

    std::vector<MeshRenderLayer> _render_layers;
    uint64_t _next_layer_id = 1;
    uint64_t _version = 0;
};

} // namespace balsa::scene_graph
