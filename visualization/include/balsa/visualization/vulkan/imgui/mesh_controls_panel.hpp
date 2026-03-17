#if !defined(BALSA_VISUALIZATION_VULKAN_IMGUI_MESH_CONTROLS_PANEL_HPP)
#define BALSA_VISUALIZATION_VULKAN_IMGUI_MESH_CONTROLS_PANEL_HPP

#include <cstddef>

namespace balsa::visualization::vulkan {

class MeshScene;
class MeshDrawable;
struct MeshRenderState;

namespace imgui {

    // Persistent state for the mesh controls panel.
    // Caller owns this and passes it to draw_*() each frame.
    struct MeshPanelState {
        int selected_drawable = -1;// -1 = nothing selected
        bool show_scene_panel = true;
        bool show_property_panel = true;
    };

    // ── Panel drawing functions ─────────────────────────────────────
    //
    // All functions assume ImGui::NewFrame() has been called and the
    // caller will call ImGui::Render() afterward.  They create their
    // own ImGui::Begin/End windows.
    //
    // Return true if any value was modified this frame (useful for
    // triggering re-renders in non-continuous-draw setups).

    // Draw the scene tree panel: lists all drawables with visibility
    // toggles and click-to-select.
    bool draw_scene_tree(MeshScene &scene, MeshPanelState &state);

    // Draw the property/controls panel for the currently-selected
    // drawable.  Shows render state controls (shading, color, lighting,
    // etc.) and per-object properties (name, visibility, transform).
    // If nothing is selected, shows a placeholder message.
    bool draw_property_panel(MeshScene &scene, MeshPanelState &state);

    // Draw render state controls for a single MeshRenderState.
    // Useful for custom layouts or when you only have one mesh.
    // Returns true if any value changed.
    bool draw_render_state_controls(MeshRenderState &state);

    // Convenience: draw both scene tree and property panel in one call.
    bool draw_mesh_controls(MeshScene &scene, MeshPanelState &state);

}// namespace imgui
}// namespace balsa::visualization::vulkan

#endif
