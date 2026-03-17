#include "balsa/visualization/vulkan/imgui/mesh_controls_panel.hpp"
#include "balsa/visualization/vulkan/mesh_scene.hpp"
#include "balsa/visualization/vulkan/mesh_drawable.hpp"
#include "balsa/visualization/vulkan/mesh_render_state.hpp"

#include <imgui.h>
#include <algorithm>
#include <array>
#include <cstring>
#include <string>

namespace balsa::visualization::vulkan::imgui {

// ── Colormap list ────────────────────────────────────────────────────
// Full list of available colormaps from the colormap_shaders subproject.
// Grouped by family for the UI combo box.

static constexpr const char *k_colormap_names[] = {
    // MATLAB family
    "MATLAB_jet",
    "MATLAB_parula",
    "MATLAB_hot",
    "MATLAB_cool",
    "MATLAB_spring",
    "MATLAB_summer",
    "MATLAB_autumn",
    "MATLAB_winter",
    "MATLAB_bone",
    "MATLAB_copper",
    "MATLAB_pink",
    "MATLAB_hsv",
    // transform family
    "transform_rainbow",
    "transform_seismic",
    "transform_hot_metal",
    "transform_grayscale_banded",
    "transform_space",
    "transform_supernova",
    "transform_ether",
    "transform_malachite",
    "transform_morning_glory",
    "transform_peanut_butter_and_jerry",
    "transform_purple_haze",
    "transform_rose",
    "transform_saturn",
    "transform_lava_waves",
    "transform_apricot",
    "transform_carnation",
    // IDL ColorBrewer (sequential / diverging)
    "IDL_CB-Blues",
    "IDL_CB-Greens",
    "IDL_CB-Greys",
    "IDL_CB-Oranges",
    "IDL_CB-Purples",
    "IDL_CB-Reds",
    "IDL_CB-BuGn",
    "IDL_CB-BuPu",
    "IDL_CB-GnBu",
    "IDL_CB-OrRd",
    "IDL_CB-PuBu",
    "IDL_CB-PuBuGn",
    "IDL_CB-PuOr",
    "IDL_CB-PuRd",
    "IDL_CB-RdBu",
    "IDL_CB-RdGy",
    "IDL_CB-RdPu",
    "IDL_CB-RdYiBu",
    "IDL_CB-RdYiGn",
    "IDL_CB-BrBG",
    "IDL_CB-PiYG",
    "IDL_CB-PRGn",
    "IDL_CB-Spectral",
    "IDL_CB-YIGn",
    "IDL_CB-YIGnBu",
    "IDL_CB-YIOrBr",
    // IDL ColorBrewer (qualitative)
    "IDL_CB-Accent",
    "IDL_CB-Dark2",
    "IDL_CB-Paired",
    "IDL_CB-Pastel1",
    "IDL_CB-Pastel2",
    "IDL_CB-Set1",
    "IDL_CB-Set2",
    "IDL_CB-Set3",
    // IDL classic
    "IDL_Rainbow",
    "IDL_Rainbow_2",
    "IDL_Rainbow_18",
    "IDL_Rainbow+Black",
    "IDL_Rainbow+White",
    "IDL_Blue-Red",
    "IDL_Blue-Red_2",
    "IDL_Blue-White_Linear",
    "IDL_Blue-Green-Red-Yellow",
    "IDL_Blue-Pastel-Red",
    "IDL_Blue_Waves",
    "IDL_Green-Pink",
    "IDL_Green-Red-Blue-White",
    "IDL_Green-White_Exponential",
    "IDL_Green-White_Linear",
    "IDL_Red-Purple",
    "IDL_Red_Temperature",
    "IDL_Black-White_Linear",
    "IDL_16_Level",
    "IDL_Beach",
    "IDL_Eos_A",
    "IDL_Eos_B",
    "IDL_Hardcandy",
    "IDL_Haze",
    "IDL_Hue_Sat_Lightness_1",
    "IDL_Hue_Sat_Lightness_2",
    "IDL_Hue_Sat_Value_1",
    "IDL_Hue_Sat_Value_2",
    "IDL_Mac_Style",
    "IDL_Nature",
    "IDL_Ocean",
    "IDL_Pastels",
    "IDL_Peppermint",
    "IDL_Plasma",
    "IDL_Prism",
    "IDL_Purple-Red+Stripes",
    "IDL_Standard_Gamma-II",
    "IDL_Steps",
    "IDL_Stern_Special",
    "IDL_Volcano",
    "IDL_Waves",
    // Other
    "gnuplot",
    "kbinani_altitude",
};
static constexpr int k_colormap_count = static_cast<int>(std::size(k_colormap_names));

// Find the index of a colormap name in the list, or -1 if not found.
static int find_colormap_index(const std::string &name) {
    for (int i = 0; i < k_colormap_count; ++i) {
        if (name == k_colormap_names[i]) return i;
    }
    return -1;
}

// ── Enum combo helpers ───────────────────────────────────────────────

static bool combo_shading_model(const char *label, ShadingModel &value) {
    static constexpr const char *items[] = { "Flat", "Gouraud", "Phong" };
    int current = static_cast<int>(value);
    if (ImGui::Combo(label, &current, items, 3)) {
        value = static_cast<ShadingModel>(current);
        return true;
    }
    return false;
}

static bool combo_render_mode(const char *label, RenderMode &value) {
    static constexpr const char *items[] = { "Solid", "Wireframe", "Points", "Solid + Wireframe" };
    int current = static_cast<int>(value);
    if (ImGui::Combo(label, &current, items, 4)) {
        value = static_cast<RenderMode>(current);
        return true;
    }
    return false;
}

static bool combo_color_source(const char *label, ColorSource &value) {
    static constexpr const char *items[] = { "Uniform Color", "Per-Vertex Color", "Scalar Field" };
    int current = static_cast<int>(value);
    if (ImGui::Combo(label, &current, items, 3)) {
        value = static_cast<ColorSource>(current);
        return true;
    }
    return false;
}

static bool combo_normal_source(const char *label, NormalSource &value) {
    static constexpr const char *items[] = { "From Attribute", "Computed in Shader", "None" };
    int current = static_cast<int>(value);
    if (ImGui::Combo(label, &current, items, 3)) {
        value = static_cast<NormalSource>(current);
        return true;
    }
    return false;
}

// ── draw_render_state_controls ───────────────────────────────────────

bool draw_render_state_controls(MeshRenderState &state) {
    bool changed = false;

    // ── Shading & Rendering ─────────────────────────────────────────
    if (ImGui::CollapsingHeader("Shading & Rendering", ImGuiTreeNodeFlags_DefaultOpen)) {
        changed |= combo_shading_model("Shading Model", state.shading);
        changed |= combo_render_mode("Render Mode", state.render_mode);
        changed |= combo_normal_source("Normal Source", state.normal_source);
        changed |= ImGui::Checkbox("Two-Sided Lighting", &state.two_sided);
    }

    // ── Color ───────────────────────────────────────────────────────
    if (ImGui::CollapsingHeader("Color", ImGuiTreeNodeFlags_DefaultOpen)) {
        changed |= combo_color_source("Color Source", state.color_source);

        if (state.color_source == ColorSource::Uniform) {
            changed |= ImGui::ColorEdit4("Uniform Color", state.uniform_color);
        }

        if (state.color_source == ColorSource::ScalarField) {
            // Colormap combo box
            int cmap_idx = find_colormap_index(state.colormap_name);
            if (ImGui::BeginCombo("Colormap", cmap_idx >= 0 ? k_colormap_names[cmap_idx] : state.colormap_name.c_str())) {
                for (int i = 0; i < k_colormap_count; ++i) {
                    bool selected = (i == cmap_idx);
                    if (ImGui::Selectable(k_colormap_names[i], selected)) {
                        state.colormap_name = k_colormap_names[i];
                        changed = true;
                    }
                    if (selected) ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }

            // Custom colormap name input
            char buf[128];
            std::strncpy(buf, state.colormap_name.c_str(), sizeof(buf) - 1);
            buf[sizeof(buf) - 1] = '\0';
            if (ImGui::InputText("Custom Colormap", buf, sizeof(buf), ImGuiInputTextFlags_EnterReturnsTrue)) {
                state.colormap_name = buf;
                changed = true;
            }

            // Scalar range
            changed |= ImGui::DragFloat("Scalar Min", &state.scalar_min, 0.01f);
            changed |= ImGui::DragFloat("Scalar Max", &state.scalar_max, 0.01f);
            if (ImGui::Button("Reset Scalar Range")) {
                state.scalar_min = 0.0f;
                state.scalar_max = 1.0f;
                changed = true;
            }
        }
    }

    // ── Lighting ────────────────────────────────────────────────────
    if (ImGui::CollapsingHeader("Lighting")) {
        changed |= ImGui::DragFloat3("Light Direction", state.light_dir, 0.01f, -1.0f, 1.0f);
        changed |= ImGui::SliderFloat("Ambient", &state.ambient_strength, 0.0f, 1.0f);
        changed |= ImGui::SliderFloat("Specular", &state.specular_strength, 0.0f, 2.0f);
        changed |= ImGui::SliderFloat("Shininess", &state.shininess, 1.0f, 256.0f, "%.0f");
    }

    // ── Wireframe / Points ──────────────────────────────────────────
    if (state.render_mode == RenderMode::Wireframe || state.render_mode == RenderMode::SolidWireframe) {
        if (ImGui::CollapsingHeader("Wireframe", ImGuiTreeNodeFlags_DefaultOpen)) {
            changed |= ImGui::ColorEdit4("Wire Color", state.wireframe_color);
            changed |= ImGui::SliderFloat("Wire Width", &state.wireframe_width, 0.5f, 5.0f);
        }
    }

    if (state.render_mode == RenderMode::Points) {
        if (ImGui::CollapsingHeader("Points", ImGuiTreeNodeFlags_DefaultOpen)) {
            changed |= ImGui::SliderFloat("Point Size", &state.point_size, 1.0f, 20.0f);
        }
    }

    return changed;
}

// ── draw_scene_tree ──────────────────────────────────────────────────

bool draw_scene_tree(MeshScene &scene, MeshPanelState &state) {
    bool changed = false;

    if (!state.show_scene_panel) return false;

    if (ImGui::Begin("Scene", &state.show_scene_panel)) {
        ImGui::Text("Drawables: %zu", scene.drawable_count());
        ImGui::Separator();

        for (std::size_t i = 0; i < scene.drawable_count(); ++i) {
            auto *d = scene.drawable(i);
            if (!d) continue;

            ImGui::PushID(static_cast<int>(i));

            // Visibility toggle
            if (ImGui::Checkbox("##vis", &d->visible)) {
                changed = true;
            }
            ImGui::SameLine();

            // Selectable name
            bool is_selected = (state.selected_drawable == static_cast<int>(i));
            if (ImGui::Selectable(d->name.c_str(), is_selected)) {
                state.selected_drawable = static_cast<int>(i);
            }

            ImGui::PopID();
        }

        // Deselect if clicked in empty area
        if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(0) && !ImGui::IsAnyItemHovered()) {
            state.selected_drawable = -1;
        }

        ImGui::Separator();

        // Add/remove buttons
        if (ImGui::Button("Add Mesh")) {
            scene.add_drawable("New Mesh");
            changed = true;
        }
        ImGui::SameLine();
        if (state.selected_drawable >= 0 && state.selected_drawable < static_cast<int>(scene.drawable_count())) {
            if (ImGui::Button("Remove Selected")) {
                scene.remove_drawable(static_cast<std::size_t>(state.selected_drawable));
                state.selected_drawable = -1;
                changed = true;
            }
        }
    }
    ImGui::End();

    return changed;
}

// ── draw_property_panel ──────────────────────────────────────────────

bool draw_property_panel(MeshScene &scene, MeshPanelState &state) {
    bool changed = false;

    if (!state.show_property_panel) return false;

    if (ImGui::Begin("Properties", &state.show_property_panel)) {
        if (state.selected_drawable < 0 || state.selected_drawable >= static_cast<int>(scene.drawable_count())) {
            ImGui::TextDisabled("No object selected");
            ImGui::End();
            return false;
        }

        auto *d = scene.drawable(static_cast<std::size_t>(state.selected_drawable));
        if (!d) {
            ImGui::TextDisabled("Invalid selection");
            ImGui::End();
            return false;
        }

        // ── Object info ─────────────────────────────────────────────
        if (ImGui::CollapsingHeader("Object", ImGuiTreeNodeFlags_DefaultOpen)) {
            // Editable name
            char name_buf[128];
            std::strncpy(name_buf, d->name.c_str(), sizeof(name_buf) - 1);
            name_buf[sizeof(name_buf) - 1] = '\0';
            if (ImGui::InputText("Name", name_buf, sizeof(name_buf))) {
                d->name = name_buf;
            }

            ImGui::Checkbox("Visible", &d->visible);

            // Mesh info
            const auto &buf = d->buffers();
            ImGui::Text("Vertices: %u", buf.vertex_count());
            ImGui::Text("Triangles: %u", buf.triangle_count());
            ImGui::Text("Edges: %u", buf.edge_count());

            ImGui::TextDisabled("Attributes:");
            ImGui::SameLine();
            if (buf.has_normals()) {
                ImGui::SameLine();
                ImGui::Text("N");
            }
            if (buf.has_colors()) {
                ImGui::SameLine();
                ImGui::Text("C");
            }
            if (buf.has_scalars()) {
                ImGui::SameLine();
                ImGui::Text("S");
            }
        }

        // ── Transform ───────────────────────────────────────────────
        if (ImGui::CollapsingHeader("Transform")) {
            // Display model matrix as translation/scale for simple editing.
            // Full matrix editing is complex; for now expose the raw mat4.
            float *m = &d->model_matrix[0][0];
            bool t_changed = false;
            t_changed |= ImGui::DragFloat4("Row 0", m + 0, 0.01f);
            t_changed |= ImGui::DragFloat4("Row 1", m + 4, 0.01f);
            t_changed |= ImGui::DragFloat4("Row 2", m + 8, 0.01f);
            t_changed |= ImGui::DragFloat4("Row 3", m + 12, 0.01f);
            if (t_changed) changed = true;

            if (ImGui::Button("Reset Transform")) {
                d->model_matrix = glm::mat4(1.0f);
                changed = true;
            }
        }

        // ── Render State ────────────────────────────────────────────
        if (ImGui::CollapsingHeader("Render State", ImGuiTreeNodeFlags_DefaultOpen)) {
            changed |= draw_render_state_controls(d->render_state);
        }
    }
    ImGui::End();

    return changed;
}

// ── draw_mesh_controls ───────────────────────────────────────────────

bool draw_mesh_controls(MeshScene &scene, MeshPanelState &state) {
    bool changed = false;
    changed |= draw_scene_tree(scene, state);
    changed |= draw_property_panel(scene, state);
    return changed;
}

}// namespace balsa::visualization::vulkan::imgui
