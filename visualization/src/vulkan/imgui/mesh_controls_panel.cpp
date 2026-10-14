#include "balsa/visualization/vulkan/imgui/mesh_controls_panel.hpp"
#include "balsa/visualization/vulkan/imgui/controls.hpp"
#include "balsa/scene_graph/Camera.hpp"
#include "balsa/scene_graph/Light.hpp"
#include "balsa/scene_graph/BasicShading.hpp"
#include "balsa/scene_graph/MeshInstance.hpp"
#include "balsa/scene_graph/Object.hpp"
#include "balsa/visualization/vulkan/mesh_render_state.hpp"
#include "balsa/visualization/vulkan/mesh_scene.hpp"

#include <algorithm>
#include <array>
#include <cstring>
#include <imgui.h>
#include <string>

namespace balsa::visualization::vulkan::imgui {



static const char *icon_for_object(const scene_graph::Object &obj) {
    if (obj.find_feature<scene_graph::Camera>()) return "[C]";
    if (obj.find_feature<scene_graph::MeshInstance>()) return "[M]";
    return "[O]"; // Empty / generic object
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

static std::unique_ptr<scene_graph::Object>
    deep_duplicate(const scene_graph::Object &src) {
    auto copy = std::make_unique<scene_graph::Object>(src.name + " Copy");
    copy->visible = src.visible;
    copy->selectable = src.selectable;
    copy->set_translation(src.translation());
    copy->set_rotation(src.rotation());
    copy->set_scale_factors(src.scale_factors());

    // Duplicate mesh features if present.  The copy shows the same
    // (shared) geometry, with its own copy of the shading.
    if (auto *instance = src.find_feature<scene_graph::MeshInstance>()) {
        copy->emplace_feature<scene_graph::MeshInstance>(
            instance->shared_geometry());
    }
    if (auto *shading = src.find_feature<scene_graph::BasicShading>()) {
        copy->emplace_feature<scene_graph::BasicShading>().copy_from(*shading);
    }

    // Recursively duplicate children
    for (auto &child : src.children()) {
        copy->add_child(deep_duplicate(*child));
    }

    return copy;
}

// Give obj and its descendants the drawables their mesh features need.
static void ensure_drawables(scene_graph::Object &obj, MeshScene &scene) {
    scene.ensure_drawable(obj);
    for (auto &child : obj.children()) ensure_drawables(*child, scene);
}


static bool draw_object_node(scene_graph::Object &obj,
                             MeshScene &scene,
                             MeshPanelState &state) {
    bool changed = false;

    ImGui::PushID(&obj);

    // ── Visibility checkbox ─────────────────────────────────────────
    // Compact checkbox before the tree node.
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(2, 2));
    if (ImGui::Checkbox("##vis", &obj.visible)) { changed = true; }
    ImGui::PopStyleVar();
    ImGui::SameLine();

    // ── Tree node ───────────────────────────────────────────────────
    bool is_selected = (state.selected_object == &obj);
    bool has_children = obj.children_count() > 0;

    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow
                               | ImGuiTreeNodeFlags_OpenOnDoubleClick
                               | ImGuiTreeNodeFlags_SpanAvailWidth;

    if (is_selected) { flags |= ImGuiTreeNodeFlags_Selected; }
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
        if (ImGui::InputText("##rename",
                             state.rename_buf,
                             sizeof(state.rename_buf),
                             ImGuiInputTextFlags_EnterReturnsTrue
                                 | ImGuiInputTextFlags_AutoSelectAll)) {
            obj.name = state.rename_buf;
            state.renaming_object = nullptr;
            changed = true;
        }
        // Cancel rename on Escape or click elsewhere
        if (ImGui::IsKeyPressed(ImGuiKey_Escape)
            || (!ImGui::IsItemActive() && ImGui::IsMouseClicked(0))) {
            state.renaming_object = nullptr;
        }
    } else {
        // Normal label — append active camera indicator
        char label[256];
        bool is_active_cam = obj.find_feature<scene_graph::Camera>()
                             && (&obj == &scene.active_camera_object());
        if (is_active_cam) {
            std::snprintf(
                label, sizeof(label), "%s %s (active)", icon, obj.name.c_str());
        } else {
            std::snprintf(
                label, sizeof(label), "%s %s", icon, obj.name.c_str());
        }
        node_open = ImGui::TreeNodeEx(label, flags);

        // Click to select
        if (ImGui::IsItemClicked(ImGuiMouseButton_Left)
            && !ImGui::IsItemToggledOpen()) {
            state.selected_object = &obj;
        }
    }

    // ── Drag source (permanent objects cannot be dragged) ──────────
    if (!obj.permanent
        && ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
        state.drag_source = &obj;
        ImGui::SetDragDropPayload(
            "SCENE_OBJECT", &state.drag_source, sizeof(scene_graph::Object *));
        ImGui::Text("%s %s", icon_for_object(obj), obj.name.c_str());
        ImGui::EndDragDropSource();
    }

    // ── Drop target ─────────────────────────────────────────────────
    if (ImGui::BeginDragDropTarget()) {
        if (auto *payload = ImGui::AcceptDragDropPayload("SCENE_OBJECT")) {
            auto *source = *static_cast<scene_graph::Object **>(payload->Data);
            // Prevent: dropping onto self, dropping onto own descendant,
            // or dropping onto current parent (no-op).
            if (source && source != &obj && source->parent() != &obj
                && !is_ancestor_of(source, &obj)) {
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
                if (detached) { obj.add_child(std::move(detached)); }
            }
            changed = true;
        }
        if (ImGui::MenuItem("Add Camera")) {
            auto &cam_child = obj.add_child("Camera");
            cam_child.emplace_feature<scene_graph::Camera>();
            changed = true;
        }

        // "Look Through" for Camera objects
        if (obj.find_feature<scene_graph::Camera>()) {
            bool is_active = (&obj == &scene.active_camera_object());
            if (is_active) {
                if (ImGui::MenuItem("Revert to Default Camera")) {
                    scene.set_active_camera(nullptr);
                    changed = true;
                }
            } else {
                if (ImGui::MenuItem("Look Through Camera")) {
                    scene.set_active_camera(&obj);
                    changed = true;
                }
            }
        }
        ImGui::Separator();
        if (!obj.permanent && ImGui::MenuItem("Rename")) {
            state.renaming_object = &obj;
            std::strncpy(state.rename_buf,
                         obj.name.c_str(),
                         sizeof(state.rename_buf) - 1);
            state.rename_buf[sizeof(state.rename_buf) - 1] = '\0';
        }
        if (!obj.permanent && ImGui::MenuItem("Duplicate")) {
            auto *parent = obj.parent();
            if (parent) {
                ensure_drawables(parent->add_child(deep_duplicate(obj)),
                                 scene);
                changed = true;
            }
        }
        ImGui::Separator();
        if (!obj.permanent && ImGui::MenuItem("Delete")) {
            // Clear selection if deleting selected
            if (state.selected_object == &obj) {
                state.selected_object = nullptr;
            }
            if (state.renaming_object == &obj) {
                state.renaming_object = nullptr;
            }
            // Use remove_mesh for mesh objects so GPU resources are freed
            auto *mesh = obj.find_feature<scene_graph::MeshInstance>();
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
            if (child.permanent) continue;
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
        if (ImGui::Button("+ Camera")) {
            auto &cam_obj = scene.root().add_child("Camera");
            cam_obj.emplace_feature<scene_graph::Camera>();
            changed = true;
        }
        ImGui::SameLine();
        if (state.selected_object && !state.selected_object->permanent) {
            if (ImGui::Button("Delete")) {
                auto *obj = state.selected_object;
                state.selected_object = nullptr;
                if (state.renaming_object == obj) {
                    state.renaming_object = nullptr;
                }
                auto *mesh = obj->find_feature<scene_graph::MeshInstance>();
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
            if (child.permanent) continue;
            changed |= draw_object_node(child, scene, state);
        }

        // Drop target on empty space → reparent to root
        ImGui::Dummy(ImGui::GetContentRegionAvail());
        if (ImGui::BeginDragDropTarget()) {
            if (auto *payload = ImGui::AcceptDragDropPayload("SCENE_OBJECT")) {
                auto *source =
                    *static_cast<scene_graph::Object **>(payload->Data);
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
        if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(0)
            && !ImGui::IsAnyItemHovered()) {
            state.selected_object = nullptr;
        }
    }
    ImGui::End();

    return changed;
}

// ── draw_property_panel ──────────────────────────────────────────────

bool draw_property_panel(MeshScene &scene, MeshPanelState &state) {
    if (!state.show_property_panel) return false;
    bool changed = false;
    ImGui::SetNextWindowPos(ImVec2(10, 320), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(380, 520), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Properties", &state.show_property_panel)) {
        changed = draw_controls({ &scene, state.selected_object });
    }
    ImGui::End();
    return changed;
}

bool draw_mesh_controls(MeshScene &scene, MeshPanelState &state) {
    bool changed = false;
    changed |= draw_scene_tree(scene, state);
    changed |= draw_property_panel(scene, state);
    return changed;
}

} // namespace balsa::visualization::vulkan::imgui
