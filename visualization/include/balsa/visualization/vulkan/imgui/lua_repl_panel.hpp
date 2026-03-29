#if !defined(BALSA_VISUALIZATION_VULKAN_IMGUI_LUA_REPL_PANEL_HPP)
#define BALSA_VISUALIZATION_VULKAN_IMGUI_LUA_REPL_PANEL_HPP

// ImGui Lua REPL panel — provides an interactive Lua console inside
// the GLFW mesh viewer.
//
// Pattern: persistent state struct + free draw function, matching
// the existing mesh_controls_panel / image_controls_panel design.

#include <string>
#include <vector>

namespace balsa::lua {
class LuaRepl;
}

namespace balsa::visualization::vulkan::imgui {

// Persistent state for the Lua REPL panel.
// Caller owns this and passes it to draw_lua_repl() each frame.
struct LuaReplPanelState {
    bool show_panel = true;

    // Input buffer for the command line.
    char input_buf[1024] = {};

    // History navigation index (-1 = not navigating).
    int history_index = -1;

    // Whether to scroll output to bottom next frame.
    bool scroll_to_bottom = true;
};

// Draw the Lua REPL panel (output area + input line with history).
//
// Assumes ImGui::NewFrame() has been called and the caller will
// call ImGui::Render() afterward.
//
// Returns true if a command was executed this frame.
auto draw_lua_repl(LuaReplPanelState &state, balsa::lua::LuaRepl &repl) -> bool;

} // namespace balsa::visualization::vulkan::imgui

#endif // BALSA_VISUALIZATION_VULKAN_IMGUI_LUA_REPL_PANEL_HPP
