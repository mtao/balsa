#include "balsa/visualization/vulkan/imgui/mesh_controls_panel.hpp"
#include "balsa/visualization/vulkan/mesh_scene.hpp"
#include "balsa/visualization/vulkan/mesh_render_state.hpp"
#include "balsa/visualization/colormap_list.hpp"
#include "balsa/scene_graph/Object.hpp"
#include "balsa/scene_graph/Camera.hpp"
#include "balsa/scene_graph/MeshData.hpp"
#include "balsa/scene_graph/Light.hpp"
#include "balsa/scene_graph/BVHData.hpp"

#include <imgui.h>
#include <algorithm>
#include <array>
#include <cstring>
#include <string>

namespace balsa::visualization::vulkan::imgui {

using visualization::k_colormap_names;
using visualization::k_colormap_count;
using visualization::find_colormap_index;

// ── Enum combo helpers ───────────────────────────────────────────────

static bool combo_shading_model(const char *label, ShadingModel &value, NormalSource normal_source) {
    static constexpr const char *items[] = { "Flat", "Gouraud", "Phong" };
    int current = static_cast<int>(value);
    if (ImGui::BeginCombo(label, items[current])) {
        for (int i = 0; i < 3; ++i) {
            // Gouraud and Phong require per-vertex normals (FromAttribute).
            // ComputedInShader gives flat per-triangle normals via dFdx/dFdy,
            // which makes smooth interpolation meaningless.
            bool item_disabled = (i > 0 && normal_source != NormalSource::FromAttribute);
            if (item_disabled) ImGui::BeginDisabled();
            bool selected = (i == current);
            if (ImGui::Selectable(items[i], selected)) {
                value = static_cast<ShadingModel>(i);
            }
            if (selected) ImGui::SetItemDefaultFocus();
            if (item_disabled) ImGui::EndDisabled();
        }
        ImGui::EndCombo();
        return value != static_cast<ShadingModel>(current);
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

static bool combo_normal_source(const char *label, NormalSource &value, bool has_normals) {
    static constexpr const char *items[] = { "From Attribute", "Computed (Flat)", "None (Unlit)" };
    int current = static_cast<int>(value);
    if (ImGui::BeginCombo(label, items[current])) {
        for (int i = 0; i < 3; ++i) {
            // "From Attribute" requires the mesh to actually have normals.
            bool item_disabled = (i == 0 && !has_normals);
            if (item_disabled) ImGui::BeginDisabled();
            bool selected = (i == current);
            if (ImGui::Selectable(items[i], selected)) {
                value = static_cast<NormalSource>(i);
            }
            if (selected) ImGui::SetItemDefaultFocus();
            if (item_disabled) ImGui::EndDisabled();
        }
        ImGui::EndCombo();
        return value != static_cast<NormalSource>(current);
    }
    return false;
}

static bool combo_cull_mode(const char *label, CullMode &value) {
    static constexpr const char *items[] = { "None (Show All)", "Back (Exterior)", "Front (Interior)" };
    int current = static_cast<int>(value);
    if (ImGui::Combo(label, &current, items, 3)) {
        value = static_cast<CullMode>(current);
        return true;
    }
    return false;
}

// ── Type icon helpers ────────────────────────────────────────────────
// Returns a short icon/prefix string based on the features attached to
// the Object.  Uses Unicode symbols that render well in most ImGui fonts.

static const char *icon_for_object(const scene_graph::Object &obj) {
    if (obj.find_feature<scene_graph::Camera>()) return "[C]";
    if (obj.find_feature<scene_graph::BVHData>()) return "[B]";
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

bool draw_render_state_controls(MeshRenderState &state, bool has_normals) {
    bool changed = false;

    // Apply constraints first — ensures UI reflects valid state
    // even if state was mutated externally.
    changed |= state.constrain(has_normals);

    // ── Shading & Rendering ─────────────────────────────────────────
    if (ImGui::CollapsingHeader("Shading & Rendering", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (combo_normal_source("Normal Source", state.normal_source, has_normals)) {
            // Re-constrain after normal source change.
            state.constrain(has_normals);
            changed = true;
        }

        // Shading model and two-sided lighting are only meaningful
        // when normals are active (i.e., lighting is enabled).
        if (state.normal_source != NormalSource::None) {
            if (combo_shading_model("Shading Model", state.shading, state.normal_source)) {
                changed = true;
            }
            changed |= ImGui::Checkbox("Two-Sided Lighting", &state.two_sided);
        }
        changed |= combo_cull_mode("Face Culling", state.cull_mode);
    }

    // ── Render Layers ───────────────────────────────────────────────
    if (ImGui::CollapsingHeader("Render Layers", ImGuiTreeNodeFlags_DefaultOpen)) {
        auto &layers = state.layers;

        // Solid layer
        ImGui::PushID("solid");
        changed |= ImGui::Checkbox("Solid", &layers.solid.enabled);
        if (layers.solid.enabled) {
            ImGui::SameLine();
            changed |= ImGui::ColorEdit4("##solid_color", layers.solid.color, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel);
        }
        ImGui::PopID();

        // Wireframe layer
        ImGui::PushID("wireframe");
        changed |= ImGui::Checkbox("Wireframe", &layers.wireframe.enabled);
        if (layers.wireframe.enabled) {
            ImGui::SameLine();
            changed |= ImGui::ColorEdit4("##wire_color", layers.wireframe.color, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel);
            changed |= ImGui::SliderFloat("Wire Width", &layers.wireframe.width, 0.5f, 5.0f);
        }
        ImGui::PopID();

        // Point layer
        ImGui::PushID("points");
        changed |= ImGui::Checkbox("Points", &layers.points.enabled);
        if (layers.points.enabled) {
            ImGui::SameLine();
            changed |= ImGui::ColorEdit4("##point_color", layers.points.color, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel);
            changed |= ImGui::SliderFloat("Point Size", &layers.points.size, 1.0f, 20.0f);
        }
        ImGui::PopID();
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

    // ── Material Response ────────────────────────────────────────────
    // Per-mesh: controls how this mesh responds to scene lighting.
    // Only shown when lighting is active (normal source is not None).
    bool is_lit = (state.normal_source != NormalSource::None);

    if (is_lit && ImGui::CollapsingHeader("Material Response")) {
        ImGui::TextDisabled("How this mesh responds to scene light");
        auto &mat = state.material;
        changed |= ImGui::SliderFloat("Ambient", &mat.ambient_strength, 0.0f, 1.0f);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Base light the mesh receives regardless of light direction");
        changed |= ImGui::SliderFloat("Diffuse", &mat.diffuse_strength, 0.0f, 2.0f);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Intensity of the diffuse lighting response");
        changed |= ImGui::SliderFloat("Specular", &mat.specular_strength, 0.0f, 2.0f);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Intensity of the specular highlight");
        changed |= ImGui::SliderFloat("Shininess", &mat.shininess, 1.0f, 256.0f, "%.0f");
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Sharpness of the specular highlight (higher = tighter)");
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

        // ── BVH Overlay ─────────────────────────────────────────────
        auto *ctx_mesh = obj.find_feature<scene_graph::MeshData>();
        if (ctx_mesh && ctx_mesh->has_triangle_indices()) {
            ImGui::Separator();
            auto *existing_bvh = obj.find_feature<scene_graph::BVHData>();
            if (!existing_bvh) {
                if (ImGui::MenuItem("Add BVH Overlay")) {
                    auto &bvh = obj.emplace_feature<scene_graph::BVHData>();
                    bvh.mark_dirty();
                    changed = true;
                }
            } else {
                if (ImGui::MenuItem("Remove BVH Overlay")) {
                    existing_bvh->mark_for_removal();
                    changed = true;
                }
            }
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
                changed |= draw_render_state_controls(mesh_data->render_state(), mesh_data->has_normals());
            }
        }

        // ── BVH Overlay ─────────────────────────────────────────────
        auto *bvh_data = obj->find_feature<scene_graph::BVHData>();
        if (bvh_data) {
            if (ImGui::CollapsingHeader("BVH Overlay", ImGuiTreeNodeFlags_DefaultOpen)) {
                // Enabled toggle
                if (ImGui::Checkbox("Show Overlay", &bvh_data->enabled)) {
                    bvh_data->mark_overlay_dirty();
                    changed = true;
                }

                if (bvh_data->enabled) {
                    ImGui::Separator();

                    // KDOP type selector
                    ImGui::Text("Bounding Volume Type:");
                    int prev_k = bvh_data->kdop_k;
                    ImGui::RadioButton("AABB (K=3)", &bvh_data->kdop_k, 3);
                    ImGui::SameLine();
                    ImGui::RadioButton("9-DOP", &bvh_data->kdop_k, 9);
                    ImGui::SameLine();
                    ImGui::RadioButton("13-DOP", &bvh_data->kdop_k, 13);
                    if (bvh_data->kdop_k != prev_k) {
                        bvh_data->mark_dirty();
                        changed = true;
                    }

                    ImGui::Separator();

                    // Build strategy selector
                    ImGui::Text("Build Strategy:");
                    static constexpr const char *strategy_names[] = {
                        "SAH (Best Quality)",
                        "Object Median (Balanced)",
                        "Spatial Median",
                        "LBVH (Fastest)"
                    };
                    int strategy_idx = static_cast<int>(bvh_data->strategy);
                    if (ImGui::Combo("##strategy", &strategy_idx, strategy_names, 4)) {
                        bvh_data->strategy = static_cast<quiver::spatial::BVHBuildStrategy>(strategy_idx);
                        bvh_data->mark_dirty();
                        changed = true;
                    }

                    // Max leaf size
                    int leaf_size = static_cast<int>(bvh_data->max_leaf_size);
                    if (ImGui::SliderInt("Max Leaf Size", &leaf_size, 1, 32)) {
                        bvh_data->max_leaf_size = static_cast<uint16_t>(leaf_size);
                        bvh_data->mark_dirty();
                        changed = true;
                    }

                    ImGui::Separator();

                    // Tree depth slider
                    if (bvh_data->is_built()) {
                        ImGui::Text("BVH height: %d", bvh_data->bvh_height());
                        int prev_depth = bvh_data->display_depth;
                        ImGui::SliderInt("Display Depth", &bvh_data->display_depth, 0, bvh_data->bvh_height());
                        if (bvh_data->display_depth != prev_depth) {
                            bvh_data->mark_overlay_dirty();
                            changed = true;
                        }
                    } else {
                        ImGui::TextDisabled("BVH not yet built");
                    }

                    ImGui::Separator();

                    // Overlay color
                    if (ImGui::ColorEdit4("BV Color", bvh_data->color)) {
                        bvh_data->mark_overlay_dirty();
                        changed = true;
                    }
                }
            }
        }
    }
    ImGui::End();

    return changed;
}

// ── draw_mesh_controls ───────────────────────────────────────────────

bool draw_scene_lighting_panel(MeshScene &scene, MeshPanelState &state) {
    bool changed = false;

    if (!state.show_lighting_panel) return false;

    if (ImGui::Begin("Scene Lighting", &state.show_lighting_panel)) {
        ImGui::TextDisabled("Global light affecting all meshes");
        auto &light = scene.headlight();

        changed |= ImGui::Checkbox("Enabled", &light.enabled);

        if (light.enabled) {
            // Direction in local space (camera space for headlight).
            float dir[3] = {
                static_cast<float>(light.direction(0)),
                static_cast<float>(light.direction(1)),
                static_cast<float>(light.direction(2))
            };
            if (ImGui::DragFloat3("Direction", dir, 0.01f, -1.0f, 1.0f)) {
                light.direction(0) = dir[0];
                light.direction(1) = dir[1];
                light.direction(2) = dir[2];
                changed = true;
            }
            ImGui::SameLine();
            ImGui::TextDisabled("(camera-local)");

            // Light color
            float col[3] = {
                static_cast<float>(light.color(0)),
                static_cast<float>(light.color(1)),
                static_cast<float>(light.color(2))
            };
            if (ImGui::ColorEdit3("Light Color", col)) {
                light.color(0) = col[0];
                light.color(1) = col[1];
                light.color(2) = col[2];
                changed = true;
            }

            if (ImGui::Button("Reset to Default")) {
                light.direction(0) = 0.0f;
                light.direction(1) = 0.0f;
                light.direction(2) = 1.0f;
                light.color(0) = 1.0f;
                light.color(1) = 1.0f;
                light.color(2) = 1.0f;
                changed = true;
            }
        }
    }
    ImGui::End();

    return changed;
}

bool draw_mesh_controls(MeshScene &scene, MeshPanelState &state) {
    bool changed = false;
    changed |= draw_scene_tree(scene, state);
    changed |= draw_property_panel(scene, state);
    changed |= draw_scene_lighting_panel(scene, state);
    return changed;
}

}// namespace balsa::visualization::vulkan::imgui
