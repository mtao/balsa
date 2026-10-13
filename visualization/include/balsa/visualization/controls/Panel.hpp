#pragma once

#include <cstdint>
#include <functional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "Layout.hpp"

namespace balsa::scene_graph {
class Object;
class MeshData;
class Light;
} // namespace balsa::scene_graph

namespace balsa::visualization::vulkan {
class MeshScene;
} // namespace balsa::visualization::vulkan

namespace balsa::visualization::controls {

struct Context {
    vulkan::MeshScene *scene = nullptr;
    scene_graph::Object *object = nullptr; ///< active (selected) object

    [[nodiscard]] auto mesh_data() const -> scene_graph::MeshData *;
    [[nodiscard]] auto headlight() const -> scene_graph::Light *;
};

struct Tab {
    std::string id; ///< e.g. "object", "data"
    std::string label;
    std::string icon; ///< single glyph used by the Qt tab bar
    int order = 0;
};

/// Register (or replace, matched by id) a tab.
auto register_tab(Tab tab) -> void;
/// Registered tabs sorted by order.
auto tabs() -> std::vector<const Tab *>;

struct Panel {
    std::string id; ///< unique, e.g. "data.render_layers"
    std::string tab_id;
    int order = 0; ///< ascending within a tab
    std::string label;
    bool default_open = true;

    std::function<bool(const Context &)> poll = [](const Context &) -> bool {
        return true;
    };
    std::function<void(const Context &, Layout &)> draw;
};

/// Register (or replace, matched by id) a panel.
auto register_panel(Panel panel) -> void;
auto unregister_panel(std::string_view id) -> bool;
auto registered_panels() -> std::span<const Panel>;

/// Panels of a tab whose poll() passes, sorted by order.
auto panels_for(const Context &ctx, std::string_view tab_id)
    -> std::vector<const Panel *>;

/// Registers the built-in tabs and panels.
/// Idempotent.
auto register_builtin_panels() -> void;

} // namespace balsa::visualization::controls
