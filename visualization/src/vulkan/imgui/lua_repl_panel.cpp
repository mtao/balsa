#include "balsa/visualization/vulkan/imgui/lua_repl_panel.hpp"
#include "balsa/lua/lua_repl.hpp"

#include <cstring>
#include <imgui.h>

namespace balsa::visualization::vulkan::imgui {

// ── History callback for ImGui::InputText ───────────────────────────

namespace {

    struct HistoryCallbackData {
        LuaReplPanelState *state;
        const balsa::lua::LuaRepl *repl;
    };

    int history_callback(ImGuiInputTextCallbackData *data) {
        auto *hcd = static_cast<HistoryCallbackData *>(data->UserData);
        auto *state = hcd->state;
        const auto &history = hcd->repl->history();

        if (history.empty()) return 0;

        if (data->EventFlag == ImGuiInputTextFlags_CallbackHistory) {
            int prev_index = state->history_index;

            if (data->EventKey == ImGuiKey_UpArrow) {
                if (state->history_index < 0) {
                    state->history_index = static_cast<int>(history.size()) - 1;
                } else if (state->history_index > 0) {
                    state->history_index--;
                }
            } else if (data->EventKey == ImGuiKey_DownArrow) {
                if (state->history_index >= 0) {
                    state->history_index++;
                    if (state->history_index
                        >= static_cast<int>(history.size())) {
                        state->history_index = -1;
                    }
                }
            }

            if (prev_index != state->history_index) {
                const char *text = (state->history_index >= 0)
                                       ? history[static_cast<std::size_t>(
                                                     state->history_index)]
                                             .c_str()
                                       : "";
                data->DeleteChars(0, data->BufTextLen);
                data->InsertChars(0, text);
            }
        }

        return 0;
    }

} // anonymous namespace

// ── Main draw function ──────────────────────────────────────────────

bool draw_lua_repl(LuaReplPanelState &state, balsa::lua::LuaRepl &repl) {
    if (!state.show_panel) return false;

    bool executed = false;

    ImGui::SetNextWindowSize(ImVec2(520, 400), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Lua REPL", &state.show_panel)) {
        // ── Output area ─────────────────────────────────────────────
        // Reserve space for the input line at the bottom.
        float footer_height = ImGui::GetStyle().ItemSpacing.y
                              + ImGui::GetFrameHeightWithSpacing();

        if (ImGui::BeginChild("##repl_output",
                              ImVec2(0, -footer_height),
                              ImGuiChildFlags_Borders,
                              ImGuiWindowFlags_HorizontalScrollbar)) {
            // Use a monospace-friendly style for output.
            const auto &output = repl.output();
            if (!output.empty()) {
                ImGui::TextUnformatted(output.c_str(),
                                       output.c_str() + output.size());
            }

            // Auto-scroll to bottom.
            if (state.scroll_to_bottom
                || ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
                ImGui::SetScrollHereY(1.0f);
            }
            state.scroll_to_bottom = false;
        }
        ImGui::EndChild();

        // ── Input line ──────────────────────────────────────────────
        ImGui::Separator();

        HistoryCallbackData hcd{&state, &repl};

        ImGuiInputTextFlags flags = ImGuiInputTextFlags_EnterReturnsTrue
                                    | ImGuiInputTextFlags_CallbackHistory;

        // Make input take most of the width.
        float clear_btn_width = ImGui::CalcTextSize("Clear").x
                                + ImGui::GetStyle().FramePadding.x * 2.0f
                                + ImGui::GetStyle().ItemSpacing.x;
        ImGui::PushItemWidth(-clear_btn_width);

        bool enter_pressed = ImGui::InputText("##repl_input",
                                              state.input_buf,
                                              sizeof(state.input_buf),
                                              flags,
                                              history_callback,
                                              &hcd);

        ImGui::PopItemWidth();

        // Keep focus on input after pressing Enter.
        if (enter_pressed) {
            std::string code(state.input_buf);
            if (!code.empty()) {
                repl.execute(code);
                state.input_buf[0] = '\0';
                state.history_index = -1;
                state.scroll_to_bottom = true;
                executed = true;
            }
            // Re-focus the input field.
            ImGui::SetKeyboardFocusHere(-1);
        }

        // Auto-focus input on first appearance.
        if (ImGui::IsWindowAppearing()) { ImGui::SetKeyboardFocusHere(-1); }

        ImGui::SameLine();
        if (ImGui::Button("Clear")) { repl.clear_output(); }
    }
    ImGui::End();

    return executed;
}

} // namespace balsa::visualization::vulkan::imgui
