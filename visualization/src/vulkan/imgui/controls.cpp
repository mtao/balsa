#include "balsa/visualization/vulkan/imgui/controls.hpp"

#include <imgui.h>

#include <array>
#include <cstring>
#include <string>

namespace balsa::visualization::vulkan::imgui {

namespace ctl = controls;
namespace sg = scene_graph;

namespace {

auto draw_property(const ctl::Property &prop) -> bool {
    const char *label = prop.label.c_str();
    const bool changed = std::visit(
        [&](const auto &v) -> bool {
            using V = std::decay_t<decltype(v)>;

            if constexpr (std::is_same_v<V, ctl::BoolInput>) {
                bool b = v.get();
                if (!ImGui::Checkbox(label, &b)) return false;
                v.set(b);
                return true;

            } else if constexpr (std::is_same_v<V, ctl::FloatInput>) {
                float x = v.get();
                const bool edited =
                    v.use_slider ? ImGui::SliderFloat(label, &x, v.min, v.max)
                             : ImGui::DragFloat(label, &x, v.step, v.min, v.max);
                if (!edited) return false;
                v.set(x);
                return true;

            } else if constexpr (std::is_same_v<V, ctl::Vec3Input>) {
                const auto cur = v.get();
                std::array<float, 3> x{ cur(0), cur(1), cur(2) };
                if (!ImGui::DragFloat3(label, x.data(), v.step, v.min, v.max))
                    return false;
                v.set(sg::Vec3f{ x[0], x[1], x[2] });
                return true;

            } else if constexpr (std::is_same_v<V, ctl::ColorInput>) {
                const auto cur = v.get();
                std::array<float, 4> c{ cur(0), cur(1), cur(2), cur(3) };
                const bool edited = v.show_alpha ? ImGui::ColorEdit4(label, c.data())
                                            : ImGui::ColorEdit3(label, c.data());
                if (!edited) return false;
                v.set(sg::Vec4f{ c[0], c[1], c[2], c[3] });
                return true;

            } else if constexpr (std::is_same_v<V, ctl::ChoiceInput>) {
                const auto options = v.options();
                const int cur = v.get_index();
                const char *preview =
                    cur >= 0 && cur < static_cast<int>(options.size())
                        ? options[static_cast<std::size_t>(cur)].c_str()
                        : "";
                bool edited = false;
                if (ImGui::BeginCombo(label, preview)) {
                    for (int i = 0; i < static_cast<int>(options.size()); ++i) {
                        ImGui::BeginDisabled(!v.option_enabled(i));
                        if (ImGui::Selectable(options[static_cast<std::size_t>(i)].c_str(),
                                              i == cur)) {
                            v.set_index(i);
                            edited = true;
                        }
                        ImGui::EndDisabled();
                    }
                    ImGui::EndCombo();
                }
                return edited;

            } else if constexpr (std::is_same_v<V, ctl::TextInput>) {
                std::array<char, 256> buf{};
                const auto cur = v.get();
                std::strncpy(buf.data(), cur.c_str(), buf.size() - 1);
                ImGui::InputText(label, buf.data(), buf.size());
                if (!ImGui::IsItemDeactivatedAfterEdit()) return false;
                v.set(std::string(buf.data()));
                return true;

            } else if constexpr (std::is_same_v<V, ctl::ReadOnlyText>) {
                ImGui::LabelText(label, "%s", v.get().c_str());
                return false;

            } else {
                static_assert(std::is_same_v<V, ctl::Button>);
                if (!ImGui::Button(label)) return false;
                v.on_click();
                return true;
            }
        },
        prop.widget);
    if (prop.live_tooltip) {
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip)) {
            ImGui::SetTooltip("%s", prop.live_tooltip().c_str());
        }
    } else if (!prop.tooltip.empty()) {
        ImGui::SetItemTooltip("%s", prop.tooltip.c_str());
    }
    return changed;
}

auto draw_layout(const ctl::Layout &layout) -> bool {
    if (!layout.visible_when()()) return false;
    const bool row = layout.direction() == ctl::Layout::Direction::Row;
    bool changed = false;

    if (!layout.heading().empty()) {
        if (row) {
            ImGui::TextUnformatted(layout.heading().c_str());
            ImGui::SameLine(90.0f);
        } else {
            ImGui::SeparatorText(layout.heading().c_str());
        }
    }

    // Rows share the width evenly between their items.
    const int count = static_cast<int>(layout.items().size());
    if (row && count > 0) ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x / count - 40.0f);

    bool first = true;
    int id = 0;
    for (const auto &item : layout.items()) {
        ImGui::PushID(id++);
        if (const auto *child = std::get_if<std::unique_ptr<ctl::Layout>>(&item)) {
            if (row && !first) ImGui::SameLine();
            changed |= draw_layout(**child);
            first = false;
        } else if (const auto &prop = std::get<ctl::Property>(item); prop.visible_when()) {
            if (row && !first) ImGui::SameLine();
            ImGui::BeginDisabled(!prop.enabled_when());
            changed |= draw_property(prop);
            ImGui::EndDisabled();
            first = false;
        }
        ImGui::PopID();
    }
    if (row && count > 0) ImGui::PopItemWidth();
    return changed;
}

} // namespace

auto draw_controls(const ctl::PanelContext &ctx) -> bool {
    ctl::register_builtin_panels();
    bool changed = false;
    if (!ImGui::BeginTabBar("##controls_tabs")) return false;
    for (const auto *tab : ctl::registered_tabs()) {
        const auto panels = ctl::available_panels(ctx, tab->id);
        if (panels.empty()) continue;
        if (!ImGui::BeginTabItem(tab->label.c_str())) continue;
        for (const auto *panel : panels) {
            ImGui::PushID(panel->id.c_str());
            const auto flags =
                panel->expanded_by_default ? ImGuiTreeNodeFlags_DefaultOpen : 0;
            if (ImGui::CollapsingHeader(panel->label.c_str(), flags)) {
                // Immediate mode: build the layout every frame.
                changed |= draw_layout(panel->build_layout(ctx));
            }
            ImGui::PopID();
        }
        ImGui::EndTabItem();
    }
    ImGui::EndTabBar();
    return changed;
}

} // namespace balsa::visualization::vulkan::imgui
