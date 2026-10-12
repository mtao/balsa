#pragma once
#include <cstddef>

namespace balsa::scene_graph {
class Object;
}

namespace balsa::visualization::vulkan {

class MeshScene;

namespace imgui {

    // Persistent state for the mesh controls panel.
    // Caller owns this and passes it to draw_*() each frame.
    struct MeshPanelState {
        scene_graph::Object *selected_object = nullptr;
        bool show_scene_panel = true;
        bool show_property_panel = true;

        // Drag-and-drop state (internal)
        scene_graph::Object *drag_source = nullptr;

        // Inline rename state (internal)
        scene_graph::Object *renaming_object = nullptr;
        char rename_buf[128] = {};
    };

    // ── Panel drawing functions ─────────────────────────────────────
    //
    // All functions assume ImGui::NewFrame() has been called and the
    // caller will call ImGui::Render() afterward.  They create their
    // own ImGui::Begin/End windows.
    //
    // Return true if any value was modified this frame (useful for
    // triggering re-renders in non-continuous-draw setups).

    // Draw the hierarchical scene graph outliner: tree of all Objects
    // with expand/collapse, visibility toggles, type icons,
    // click-to-select, drag-and-drop reparenting, right-click context
    // menu (Add Child, Add Mesh, Rename, Duplicate, Delete), and
    // inline rename (double-click or F2).
    bool draw_scene_tree(MeshScene &scene, MeshPanelState &state);

    // Draw the registered control panels (see controls/Panel.hpp) for the
    // selected object and scene.
    bool draw_property_panel(MeshScene &scene, MeshPanelState &state);

    // Convenience: draw both scene tree and property panel in one call.
    bool draw_mesh_controls(MeshScene &scene, MeshPanelState &state);

}// namespace imgui
}// namespace balsa::visualization::vulkan
