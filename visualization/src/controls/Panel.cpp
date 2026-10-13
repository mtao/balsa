#include "balsa/visualization/controls/Panel.hpp"

#include <algorithm>

#include "balsa/scene_graph/Light.hpp"
#include "balsa/scene_graph/BasicShading.hpp"
#include "balsa/scene_graph/MeshGeometry.hpp"
#include "balsa/scene_graph/Object.hpp"
#include "balsa/visualization/vulkan/mesh_scene.hpp"

namespace balsa::visualization::controls {

auto PanelContext::active_geometry() const -> scene_graph::MeshGeometry * {
    return active_object
               ? active_object->find_feature<scene_graph::MeshGeometry>()
               : nullptr;
}

auto PanelContext::active_shading() const -> scene_graph::BasicShading * {
    return active_object
               ? active_object->find_feature<scene_graph::BasicShading>()
               : nullptr;
}

auto PanelContext::headlight() const -> scene_graph::Light * {
    return scene ? &scene->headlight() : nullptr;
}

namespace {
auto registry() -> std::vector<Panel> & {
    static std::vector<Panel> r;
    return r;
}
auto tab_registry() -> std::vector<Tab> & {
    static std::vector<Tab> r;
    return r;
}

template <typename T>
auto upsert(std::vector<T> &v, T item) -> void {
    if (auto it = std::ranges::find(v, item.id, &T::id); it != v.end()) {
        *it = std::move(item);
    } else {
        v.push_back(std::move(item));
    }
}
} // namespace

auto register_tab(Tab tab) -> void { upsert(tab_registry(), std::move(tab)); }

auto registered_tabs() -> std::vector<const Tab *> {
    std::vector<const Tab *> out;
    for (const auto &t : tab_registry()) out.push_back(&t);
    std::ranges::stable_sort(out, {}, &Tab::sort_key);
    return out;
}

auto register_panel(Panel panel) -> void {
    upsert(registry(), std::move(panel));
}

auto unregister_panel(std::string_view id) -> bool {
    return std::erase_if(registry(),
                         [id](const Panel &p) { return p.id == id; })
           > 0;
}

auto registered_panels() -> std::span<const Panel> { return registry(); }

auto available_panels(const PanelContext &ctx, std::string_view tab_id) -> std::vector<const Panel *> {
    std::vector<const Panel *> out;
    for (const auto &p : registry()) {
        if (p.tab_id == tab_id && p.is_available(ctx)) out.push_back(&p);
    }
    std::ranges::stable_sort(out, {}, &Panel::sort_key);
    return out;
}

} // namespace balsa::visualization::controls
