#include "balsa/visualization/vulkan/imgui/mesh_controls_panel.hpp"
#include "balsa/visualization/vulkan/mesh_scene.hpp"
#include "balsa/visualization/vulkan/mesh_render_state.hpp"
#include "balsa/scene_graph/Object.hpp"
#include "balsa/scene_graph/Camera.hpp"
#include "balsa/scene_graph/MeshData.hpp"

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

// ── Type icon helpers ────────────────────────────────────────────────
// Returns a short icon/prefix string based on the features attached to
// the Object.  Uses Unicode symbols that render well in most ImGui fonts.

static const char *icon_for_object(const scene_graph::Object &obj) {
    if (obj.find_feature<scene_graph::Camera>()) return "[C]";
    if (obj.find_feature<scene_graph::MeshData>()) return "[M]";
    return "[O]";// Empty / generic object
}

// ── Ancestor check (for drag-drop loop prevention) ──────────────────

static bool is_ancestor_of(const scene_graph::Object *candidate,
                           const scene_graph::Object *node) {
    for (auto *p = node->parent(); p != nullptr; p = p->parent()) {
        if (p == candidate) return true;
    }
    return false;
}

// ── Deep duplicate ──────────────────────────────────────────────────

static std::unique_ptr<scene_graph::Object> deep_duplicate(
  const scene_graph::Object &src,
  MeshScene &scene) {
    auto copy = std::make_unique<scene_graph::Object>(src.name + " Copy");
    copy->visible = src.visible;
    copy->selectable = src.selectable;
    copy->set_translation(src.translation());
    copy->set_rotation(src.rotation());
    copy->set_scale_factors(src.scale_factors());

    // Duplicate MeshData feature if present
    if (auto *md = src.find_feature<scene_graph::MeshData>()) {
        auto &new_md = copy->emplace_feature<scene_graph::MeshData>();
        new_md.render_state() = md->render_state();
        // Copy geometry data via the public setters
        // (positions, normals, etc. would need accessor APIs to fully
        //  copy; for now the render state is the most important part)
    }

    // Recursively duplicate children
    for (auto &child : src.children()) {
        copy->add_child(deep_duplicate(*child, scene));
    }

    return copy;
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

// ── draw_object_node (recursive) ─────────────────────────────────────
//
// Draws a single Object as a tree node, then recurses into children.
// Handles:
//   - TreeNodeEx with expand/collapse
//   - Type icon prefix ([M] mesh, [C] camera, [O] empty)
//   - Click-to-select (single selection)
//   - Visibility checkbox (eye toggle)
//   - Inline rename (double-click or context menu)
//   - Drag-and-drop reparenting with ancestor-loop protection
//   - Right-click context menu (Add Child, Add Mesh, Rename,
//     Duplicate, Delete)

static bool draw_object_node(scene_graph::Object &obj,
                             MeshScene &scene,
                             MeshPanelState &state) {
    bool changed = false;

    ImGui::PushID(&obj);

    // ── Visibility checkbox ─────────────────────────────────────────
    // Compact checkbox before the tree node.
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(2, 2));
    if (ImGui::Checkbox("##vis", &obj.visible)) {
        changed = true;
    }
    ImGui::PopStyleVar();
    ImGui::SameLine();

    // ── Tree node ───────────────────────────────────────────────────
    bool is_selected = (state.selected_object == &obj);
    bool has_children = obj.children_count() > 0;

    ImGuiTreeNodeFlags flags =
      ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_SpanAvailWidth;

    if (is_selected) {
        flags |= ImGuiTreeNodeFlags_Selected;
    }
    if (!has_children) {
        flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
    }

    // Build label: icon + name
    const char *icon = icon_for_object(obj);

    // If this node is being renamed inline, show InputText instead
    bool is_renaming = (state.renaming_object == &obj);
    bool node_open = false;

    if (is_renaming) {
        // Show tree arrow/indent but use InputText for the label
        node_open = ImGui::TreeNodeEx("##rename_node", flags);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(-1);
        ImGui::SetKeyboardFocusHere();
        if (ImGui::InputText("##rename", state.rename_buf, sizeof(state.rename_buf), ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll)) {
            obj.name = state.rename_buf;
            state.renaming_object = nullptr;
            changed = true;
        }
        // Cancel rename on Escape or click elsewhere
        if (ImGui::IsKeyPressed(ImGuiKey_Escape) || (!ImGui::IsItemActive() && ImGui::IsMouseClicked(0))) {
            state.renaming_object = nullptr;
        }
    } else {
        // Normal label
        char label[256];
        std::snprintf(label, sizeof(label), "%s %s", icon, obj.name.c_str());
        node_open = ImGui::TreeNodeEx(label, flags);

        // Click to select
        if (ImGui::IsItemClicked(ImGuiMouseButton_Left) && !ImGui::IsItemToggledOpen()) {
            state.selected_object = &obj;
        }
    }

    // ── Drag source ─────────────────────────────────────────────────
    if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
        state.drag_source = &obj;
        ImGui::SetDragDropPayload("SCENE_OBJECT", &state.drag_source, sizeof(scene_graph::Object *));
        ImGui::Text("%s %s", icon_for_object(obj), obj.name.c_str());
        ImGui::EndDragDropSource();
    }

    // ── Drop target ─────────────────────────────────────────────────
    if (ImGui::BeginDragDropTarget()) {
        if (auto *payload = ImGui::AcceptDragDropPayload("SCENE_OBJECT")) {
            auto *source = *static_cast<scene_graph::Object **>(payload->Data);
            // Prevent: dropping onto self, dropping onto own descendant,
            // or dropping onto current parent (no-op).
            if (source && source != &obj && source->parent() != &obj && !is_ancestor_of(source, &obj)) {
                auto detached = source->detach();
                if (detached) {
                    obj.add_child(std::move(detached));
                    changed = true;
                }
            }
        }
        ImGui::EndDragDropTarget();
    }

    // ── Context menu ────────────────────────────────────────────────
    if (ImGui::BeginPopupContextItem()) {
        state.selected_object = &obj;

        if (ImGui::MenuItem("Add Child (Empty)")) {
            obj.add_child("Empty");
            changed = true;
        }
        if (ImGui::MenuItem("Add Mesh")) {
            auto &mesh_obj = scene.add_mesh("New Mesh");
            // Reparent under this node if not root
            if (&obj != &scene.root()) {
                auto detached = mesh_obj.detach();
                if (detached) {
                    obj.add_child(std::move(detached));
                }
            }
            changed = true;
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Rename")) {
            state.renaming_object = &obj;
            std::strncpy(state.rename_buf, obj.name.c_str(), sizeof(state.rename_buf) - 1);
            state.rename_buf[sizeof(state.rename_buf) - 1] = '\0';
        }
        if (ImGui::MenuItem("Duplicate")) {
            auto *parent = obj.parent();
            if (parent) {
                parent->add_child(deep_duplicate(obj, scene));
                changed = true;
            }
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Delete")) {
            // Clear selection if deleting selected
            if (state.selected_object == &obj) {
                state.selected_object = nullptr;
            }
            if (state.renaming_object == &obj) {
                state.renaming_object = nullptr;
            }
            // Use remove_mesh for mesh objects so GPU resources are freed
            auto *mesh = obj.find_feature<scene_graph::MeshData>();
            if (mesh) {
                scene.remove_mesh(&obj);
            } else {
                auto detached = obj.detach();
                // detached unique_ptr goes out of scope and destroys the object
            }
            changed = true;
            ImGui::EndPopup();
            ImGui::PopID();
            return changed;
        }

        ImGui::EndPopup();
    }

    // ── Recurse into children ───────────────────────────────────────
    if (node_open && has_children) {
        for (std::size_t i = 0; i < obj.children_count(); ++i) {
            auto &child = *obj.children()[i];
            changed |= draw_object_node(child, scene, state);
        }
        ImGui::TreePop();
    }

    ImGui::PopID();
    return changed;
}

// ── draw_scene_tree ──────────────────────────────────────────────────

bool draw_scene_tree(MeshScene &scene, MeshPanelState &state) {
    bool changed = false;

    if (!state.show_scene_panel) return false;

    if (ImGui::Begin("Scene", &state.show_scene_panel)) {
        // Toolbar row
        if (ImGui::Button("+ Empty")) {
            scene.root().add_child("Empty");
            changed = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("+ Mesh")) {
            scene.add_mesh("New Mesh");
            changed = true;
        }
        ImGui::SameLine();
        if (state.selected_object) {
            if (ImGui::Button("Delete")) {
                auto *obj = state.selected_object;
                state.selected_object = nullptr;
                if (state.renaming_object == obj) {
                    state.renaming_object = nullptr;
                }
                auto *mesh = obj->find_feature<scene_graph::MeshData>();
                if (mesh) {
                    scene.remove_mesh(obj);
                } else {
                    auto detached = obj->detach();
                }
                changed = true;
            }
        } else {
            ImGui::BeginDisabled();
            ImGui::Button("Delete");
            ImGui::EndDisabled();
        }

        ImGui::Separator();

        // ── Drop target on the root area ────────────────────────────
        // Allows dropping objects to reparent them under root.

        // Draw all root children as tree nodes
        auto &root = scene.root();
        for (std::size_t i = 0; i < root.children_count(); ++i) {
            auto &child = *root.children()[i];
            // Skip camera node (internal, not user-editable)
            if (child.find_feature<scene_graph::Camera>()) continue;
            changed |= draw_object_node(child, scene, state);
        }

        // Drop target on empty space → reparent to root
        ImGui::Dummy(ImGui::GetContentRegionAvail());
        if (ImGui::BeginDragDropTarget()) {
            if (auto *payload = ImGui::AcceptDragDropPayload("SCENE_OBJECT")) {
                auto *source = *static_cast<scene_graph::Object **>(payload->Data);
                if (source && source->parent() != &root) {
                    auto detached = source->detach();
                    if (detached) {
                        root.add_child(std::move(detached));
                        changed = true;
                    }
                }
            }
            ImGui::EndDragDropTarget();
        }

        // Deselect if clicked in empty area
        if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(0) && !ImGui::IsAnyItemHovered()) {
            state.selected_object = nullptr;
        }
    }
    ImGui::End();

    return changed;
}

// ── draw_property_panel ──────────────────────────────────────────────

bool draw_property_panel(MeshScene & /*scene*/, MeshPanelState &state) {
    bool changed = false;

    if (!state.show_property_panel) return false;

    if (ImGui::Begin("Properties", &state.show_property_panel)) {
        if (!state.selected_object) {
            ImGui::TextDisabled("No object selected");
            ImGui::End();
            return false;
        }

        auto *obj = state.selected_object;
        auto *mesh_data = obj->find_feature<scene_graph::MeshData>();

        // ── Object info ─────────────────────────────────────────────
        if (ImGui::CollapsingHeader("Object", ImGuiTreeNodeFlags_DefaultOpen)) {
            // Editable name
            char name_buf[128];
            std::strncpy(name_buf, obj->name.c_str(), sizeof(name_buf) - 1);
            name_buf[sizeof(name_buf) - 1] = '\0';
            if (ImGui::InputText("Name", name_buf, sizeof(name_buf))) {
                obj->name = name_buf;
            }

            ImGui::Checkbox("Visible", &obj->visible);
            ImGui::SameLine();
            ImGui::Checkbox("Selectable", &obj->selectable);

            // Mesh info (from MeshData)
            if (mesh_data) {
                ImGui::Text("Vertices: %zu", mesh_data->vertex_count());
                ImGui::Text("Triangles: %zu", mesh_data->triangle_count());
                ImGui::Text("Edges: %zu", mesh_data->edge_count());

                ImGui::TextDisabled("Attributes:");
                if (mesh_data->has_normals()) {
                    ImGui::SameLine();
                    ImGui::Text("N");
                }
                if (mesh_data->has_vertex_colors()) {
                    ImGui::SameLine();
                    ImGui::Text("C");
                }
                if (mesh_data->has_scalar_field()) {
                    ImGui::SameLine();
                    ImGui::Text("S");
                }
            } else {
                ImGui::TextDisabled("No MeshData");
            }
        }

        // ── Transform ───────────────────────────────────────────────
        if (ImGui::CollapsingHeader("Transform")) {
            // Display decomposed TRS for editing.
            auto t = obj->translation();
            auto euler = obj->rotation_euler();
            auto s = obj->scale_factors();

            // We need contiguous float[3] for DragFloat3, so copy into
            // local arrays, edit, then set back.
            float tv[3] = {
                static_cast<float>(t(0)),
                static_cast<float>(t(1)),
                static_cast<float>(t(2))
            };
            float rv[3] = {
                static_cast<float>(euler(0)),
                static_cast<float>(euler(1)),
                static_cast<float>(euler(2))
            };
            float sv[3] = {
                static_cast<float>(s(0)),
                static_cast<float>(s(1)),
                static_cast<float>(s(2))
            };

            bool t_changed = false;
            t_changed |= ImGui::DragFloat3("Translation", tv, 0.01f);
            t_changed |= ImGui::DragFloat3("Rotation", rv, 0.5f);
            t_changed |= ImGui::DragFloat3("Scale", sv, 0.01f);

            if (t_changed) {
                scene_graph::Vec3f new_t;
                new_t(0) = tv[0];
                new_t(1) = tv[1];
                new_t(2) = tv[2];
                obj->set_translation(new_t);

                scene_graph::Vec3f new_r;
                new_r(0) = rv[0];
                new_r(1) = rv[1];
                new_r(2) = rv[2];
                obj->set_rotation_euler(new_r);

                scene_graph::Vec3f new_s;
                new_s(0) = sv[0];
                new_s(1) = sv[1];
                new_s(2) = sv[2];
                obj->set_scale_factors(new_s);

                changed = true;
            }

            if (ImGui::Button("Reset Transform")) {
                obj->reset_transform();
                changed = true;
            }
        }

        // ── Render State ────────────────────────────────────────────
        if (mesh_data) {
            if (ImGui::CollapsingHeader("Render State", ImGuiTreeNodeFlags_DefaultOpen)) {
                changed |= draw_render_state_controls(mesh_data->render_state());
            }
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
