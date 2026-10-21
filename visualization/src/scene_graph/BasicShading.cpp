#include "balsa/scene_graph/BasicShading.hpp"

#include <spdlog/spdlog.h>

#include "balsa/scene_graph/MeshGeometry.hpp"
#include "balsa/scene_graph/MeshInstance.hpp"
#include "balsa/scene_graph/Object.hpp"

namespace balsa::scene_graph {

BasicShading::BasicShading() : _render_layers(make_default_mesh_render_layers()) {
    for (auto &layer : _render_layers) layer.id = {_next_layer_id++};
}

auto BasicShading::geometry() const -> const MeshGeometry * {
    if (!is_attached()) return nullptr;
    const auto *instance = object().find_feature<MeshInstance>();
    return instance ? instance->geometry() : nullptr;
}

auto BasicShading::copy_from(const BasicShading &other) -> void {
    if (&other == this) return;
    _render_layers = other._render_layers;
    _next_layer_id = other._next_layer_id;
    // Re-target explicit bindings so they do not keep other's data alive.
    // Unresolvable ones keep their attribute: layer_attribute() resolves
    // by name, so they bind if a same-named attribute appears later.
    if (const auto *g = geometry()) {
        for (auto &layer : _render_layers) {
            for (auto &[role, binding] : layer.bindings) {
                if (!binding.attribute) continue;
                if (auto a = g->find_attribute(binding.attribute->name)) {
                    binding.attribute = std::move(a);
                }
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
                                   std::string_view role) const
    -> MeshAttributeHandle {
    const auto *g = geometry();
    if (g == nullptr || !layer.renderer) return {};
    const auto *requirement = layer.renderer->requirement(role);
    if (requirement == nullptr) return {};

    MeshAttributeHandle attribute;
    if (const auto *binding = layer.binding(role)) {
        if (!binding->attribute) return {}; // explicitly unbound
        attribute = g->owns(binding->attribute)
                        ? binding->attribute
                        : g->find_attribute(binding->attribute->name);
    } else if (!requirement->default_attribute.empty()) {
        attribute = g->find_attribute(requirement->default_attribute);
    }
    return attribute && requirement->accepts(*attribute) ? attribute
                                                         : MeshAttributeHandle{};
}

auto BasicShading::set_binding(MeshLayerId layer_id, std::string_view role,
                               std::optional<MeshRoleBinding> binding) -> bool {
    auto *layer = find_layer(layer_id);
    if (layer == nullptr || !layer->renderer
        || layer->renderer->requirement(role) == nullptr) {
        return false;
    }
    if (binding) {
        layer->bindings.insert_or_assign(std::string(role), std::move(*binding));
    } else {
        layer->bindings.erase(std::string(role));
    }
    ++_version;
    return true;
}

auto BasicShading::assign_layer_role(MeshLayerId layer_id, std::string_view role,
                                     const MeshAttributeHandle &attribute) -> bool {
    const auto *g = geometry();
    const auto *layer = find_layer(layer_id);
    if (g == nullptr || layer == nullptr || !layer->renderer || !g->owns(attribute)) {
        return false;
    }
    const auto *requirement = layer->renderer->requirement(role);
    if (requirement == nullptr) return false;
    if (!requirement->accepts(*attribute)) {
        spdlog::warn("BasicShading: attribute '{}' ({} components) cannot bind "
                     "role '{}' of '{}'",
                     attribute->name, attribute->component_count(), role,
                     layer->renderer->id());
        return false;
    }
    return set_binding(layer_id, role, MeshRoleBinding{attribute});
}

auto BasicShading::assign_layer_role(MeshLayerId layer_id, std::string_view role,
                                     std::string_view attribute_name) -> bool {
    const auto *g = geometry();
    return g != nullptr
           && assign_layer_role(layer_id, role, g->find_attribute(attribute_name));
}

auto BasicShading::clear_layer_role(MeshLayerId layer_id, std::string_view role) -> bool {
    return set_binding(layer_id, role, MeshRoleBinding{});
}

auto BasicShading::reset_layer_role(MeshLayerId layer_id, std::string_view role) -> bool {
    return set_binding(layer_id, role, std::nullopt);
}

auto MeshLayerContext::attribute(std::string_view role) const -> MeshAttributeHandle {
    const auto *layer = m_shading ? m_shading->find_layer(m_layer) : nullptr;
    return layer ? m_shading->layer_attribute(*layer, role) : MeshAttributeHandle{};
}

auto MeshLayerContext::geometry_version() const -> uint64_t {
    const auto *geometry = m_shading ? m_shading->geometry() : nullptr;
    return geometry ? geometry->version() : 0;
}

} // namespace balsa::scene_graph
