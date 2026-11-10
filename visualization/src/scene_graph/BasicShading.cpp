#include "balsa/scene_graph/BasicShading.hpp"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <utility>
#include <ranges>

#include "balsa/scene_graph/MeshGeometry.hpp"
#include "balsa/scene_graph/MeshInstance.hpp"
#include "balsa/scene_graph/Object.hpp"

namespace balsa::scene_graph {

BasicShading::BasicShading() : BasicShading(make_default_mesh_render_layers()) {}

BasicShading::BasicShading(std::vector<MeshRenderLayer> layers)
  : _render_layers(std::move(layers)) {
    for (auto &layer : _render_layers) layer.id = {_next_layer_id++};
}

auto BasicShading::geometry() const -> const MeshGeometry * {
    if (!is_attached()) return nullptr;
    const auto *instance = object().find_feature<MeshInstance>();
    return instance ? instance->geometry() : nullptr;
}

auto BasicShading::layer_geometry(const MeshRenderLayer &layer) const
    -> std::shared_ptr<const MeshGeometry> {
    if (!is_attached()) return nullptr;
    const auto *instance = object().find_feature<MeshInstance>();
    return instance ? instance->view(layer.view) : nullptr;
}

auto BasicShading::fit_to_geometry() -> void {
    if (geometry() == nullptr) return;
    for (auto &layer : _render_layers) {
        if (layer.enabled && layer.renderer
            && !layer.renderer->fit_to({*this, layer.id})) {
            layer.enabled = false;
        }
    }
    ++_version;
}

auto BasicShading::copy_from(const BasicShading &other) -> void {
    if (&other == this) return;
    _render_layers = other._render_layers;
    _active_layer = other._active_layer;
    _next_layer_id = other._next_layer_id;
    // Re-target explicit bindings so they do not keep other's data alive.
    // Unresolvable ones keep their attribute: layer_attribute() resolves
    // by name, so they bind if a same-named attribute appears later.
    for (auto &layer : _render_layers) {
        const auto g = layer_geometry(layer);
        if (!g) continue;
        for (auto &[role, binding] : layer.bindings) {
            if (!binding.attribute) continue;
            if (auto a = g->find_attribute(binding.attribute->name)) {
                binding.attribute = std::move(a);
            }
        }
    }
    ++_version;
}

auto BasicShading::find_layer(MeshLayerId id) -> MeshRenderLayer * {
    const auto it = std::ranges::find(_render_layers, id, &MeshRenderLayer::id);
    return it == _render_layers.end() ? nullptr : &*it;
}

auto BasicShading::find_layer(MeshLayerId id) const -> const MeshRenderLayer * {
    const auto it = std::ranges::find(_render_layers, id, &MeshRenderLayer::id);
    return it == _render_layers.end() ? nullptr : &*it;
}

auto BasicShading::active_layer() -> MeshRenderLayer * {
    return const_cast<MeshRenderLayer *>(std::as_const(*this).active_layer());
}

auto BasicShading::active_layer() const -> const MeshRenderLayer * {
    if (const auto *layer = find_layer(_active_layer)) return layer;
    return _render_layers.empty() ? nullptr : &_render_layers.front();
}

auto BasicShading::set_active_layer(MeshLayerId id) -> bool {
    if (find_layer(id) == nullptr) return false;
    _active_layer = id;
    return true;
}

auto BasicShading::find_layer(std::string_view renderer_id) -> MeshRenderLayer * {
    const auto it = std::ranges::find_if(_render_layers, [renderer_id](const auto &layer) {
        return layer.renderer && layer.renderer->id() == renderer_id;
    });
    return it == _render_layers.end() ? nullptr : &*it;
}

auto BasicShading::find_layer(std::string_view renderer_id) const
    -> const MeshRenderLayer * {
    const auto it = std::ranges::find_if(_render_layers, [renderer_id](const auto &layer) {
        return layer.renderer && layer.renderer->id() == renderer_id;
    });
    return it == _render_layers.end() ? nullptr : &*it;
}

auto BasicShading::layer_attribute(const MeshRenderLayer &layer,
                                   MeshRole role) const
    -> MeshAttributeHandle {
    const auto g = layer_geometry(layer);
    if (g == nullptr || !layer.renderer) return {};
    const auto *requirement = layer.renderer->requirement(role);
    if (requirement == nullptr) return {};

    MeshAttributeHandle attribute;
    if (const auto *binding = layer.binding(role)) {
        if (!binding->attribute) return {}; // explicitly unbound
        attribute = g->owns(binding->attribute)
                        ? binding->attribute
                        : g->find_attribute(binding->attribute->name);
    } else if (requirement->semantic) {
        attribute = g->attribute_for(*requirement->semantic, requirement->set);
    }
    return attribute && requirement->accepts(*attribute) ? attribute
                                                         : MeshAttributeHandle{};
}

auto BasicShading::candidates(const MeshRenderLayer &layer,
                              MeshRole role) const
    -> std::vector<MeshAttributeHandle> {
    const auto g = layer_geometry(layer);
    const auto *requirement = layer.renderer ? layer.renderer->requirement(role) : nullptr;
    if (g == nullptr || requirement == nullptr) return {};
    const auto fits = [&](const MeshAttributeHandle &a) {
        return !MeshGeometry::is_reserved_name(a->name) && requirement->accepts(*a);
    };

    std::vector<MeshAttributeHandle> result;
    if (requirement->semantic) {
        auto tagged = std::ranges::to<std::vector>(
            g->semantics() | std::views::filter([&](const auto &entry) {
                return entry.semantic == requirement->semantic;
            }));
        std::ranges::sort(tagged, {}, &MeshGeometry::SemanticEntry::set);
        for (const auto &entry : tagged) {
            if (fits(entry.attribute) && !std::ranges::contains(result, entry.attribute)) {
                result.push_back(entry.attribute);
            }
        }
    }
    for (const auto &a : g->attributes()) {
        if (fits(a) && !std::ranges::contains(result, a)) result.push_back(a);
    }
    return result;
}

auto BasicShading::set_binding(MeshLayerId layer_id, MeshRole role,
                               std::optional<MeshRoleBinding> binding) -> bool {
    auto *layer = find_layer(layer_id);
    if (layer == nullptr || !layer->renderer
        || layer->renderer->requirement(role) == nullptr) {
        return false;
    }
    if (binding) {
        layer->bindings.insert_or_assign(role, std::move(*binding));
    } else {
        layer->bindings.erase(role);
    }
    ++_version;
    return true;
}

auto BasicShading::assign_layer_role(MeshLayerId layer_id, MeshRole role,
                                     const MeshAttributeHandle &attribute) -> bool {
    const auto *layer = find_layer(layer_id);
    const auto g = layer ? layer_geometry(*layer) : nullptr;
    if (g == nullptr || layer == nullptr || !layer->renderer || !g->owns(attribute)) {
        return false;
    }
    const auto *requirement = layer->renderer->requirement(role);
    if (requirement == nullptr) return false;
    if (!requirement->accepts(*attribute)) {
        spdlog::warn("BasicShading: attribute '{}' ({} components) cannot bind "
                     "role '{}' of '{}'",
                     attribute->name, attribute->component_count(), role.id(),
                     layer->renderer->id());
        return false;
    }
    return set_binding(layer_id, role, MeshRoleBinding{attribute});
}

auto BasicShading::assign_layer_role(MeshLayerId layer_id, MeshRole role,
                                     std::string_view attribute_name) -> bool {
    const auto *layer = find_layer(layer_id);
    const auto g = layer ? layer_geometry(*layer) : nullptr;
    return g != nullptr
           && assign_layer_role(layer_id, role, g->find_attribute(attribute_name));
}

auto BasicShading::clear_layer_role(MeshLayerId layer_id, MeshRole role) -> bool {
    return set_binding(layer_id, role, MeshRoleBinding{});
}

auto BasicShading::reset_layer_role(MeshLayerId layer_id, MeshRole role) -> bool {
    return set_binding(layer_id, role, std::nullopt);
}

auto MeshLayerContext::attribute(MeshRole role) const -> MeshAttributeHandle {
    const auto *layer = m_shading ? m_shading->find_layer(m_layer) : nullptr;
    return layer ? m_shading->layer_attribute(*layer, role) : MeshAttributeHandle{};
}

auto MeshLayerContext::candidates(MeshRole role) const
    -> std::vector<MeshAttributeHandle> {
    const auto *layer = m_shading ? m_shading->find_layer(m_layer) : nullptr;
    return layer ? m_shading->candidates(*layer, role)
                 : std::vector<MeshAttributeHandle>{};
}

auto MeshLayerContext::geometry() const -> const MeshGeometry * {
    const auto *layer = m_shading ? m_shading->find_layer(m_layer) : nullptr;
    // The MeshInstance caches the view, so the pointer outlives this call.
    return layer ? m_shading->layer_geometry(*layer).get() : nullptr;
}

auto MeshLayerContext::geometry_version() const -> uint64_t {
    // Views are rebuilt from the shading's geometry, so its version covers
    // them.
    const auto *geometry = m_shading ? m_shading->geometry() : nullptr;
    return geometry ? geometry->version() : 0;
}

} // namespace balsa::scene_graph
