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
class MeshGeometry;
class BasicShading;
class Light;
} // namespace balsa::scene_graph

namespace balsa::visualization::vulkan {
class MeshScene;
} // namespace balsa::visualization::vulkan

namespace balsa::visualization::controls {

/// What the panels describe: the scene and the current selection.
struct PanelContext {
    vulkan::MeshScene *scene = nullptr;
    scene_graph::Object *active_object = nullptr; ///< selected object, if any

    /// Features of the selected object, or nullptr.
    [[nodiscard]] auto active_geometry() const -> scene_graph::MeshGeometry *;
    [[nodiscard]] auto active_shading() const -> scene_graph::BasicShading *;
    [[nodiscard]] auto headlight() const -> scene_graph::Light *;
};

struct Tab {
    std::string id; ///< e.g. "object", "data"
    std::string label;
    std::string icon_glyph; ///< single character shown on the Qt tab bar
    int sort_key = 0; ///< tabs are sorted ascending; ties keep registration order
};

/// Register (or replace, matched by id) a tab.
auto register_tab(Tab tab) -> void;
/// Registered tabs sorted by sort_key.
auto registered_tabs() -> std::vector<const Tab *>;

struct Panel {
    std::string id; ///< unique, e.g. "data.render_layers"
    std::string tab_id;
    int sort_key = 0; ///< panels in a tab are sorted ascending; ties keep registration order
    std::string label;
    bool expanded_by_default = true;

    /// Whether the panel applies to the context (e.g. a mesh is selected).
    std::function<bool(const PanelContext &)> is_available =
        [](const PanelContext &) -> bool { return true; };
    /// The panel's properties for the context; only called when
    /// is_available() passes.
    std::function<Layout(const PanelContext &)> build_layout;
};

/// Register (or replace, matched by id) a panel.
auto register_panel(Panel panel) -> void;
auto unregister_panel(std::string_view id) -> bool;
auto registered_panels() -> std::span<const Panel>;

/// Panels of a tab whose is_available() passes, sorted by sort_key.
auto available_panels(const PanelContext &ctx, std::string_view tab_id)
    -> std::vector<const Panel *>;

/// Registers the built-in tabs and panels.
/// Idempotent.
auto register_builtin_panels() -> void;

} // namespace balsa::visualization::controls
