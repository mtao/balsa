#include "balsa/visualization/vulkan/mesh_scene.hpp"
#include "balsa/visualization/vulkan/vulkan_mesh_drawable.hpp"
#include "balsa/visualization/vulkan/film.hpp"
#include "balsa/glm/zipper_compat.hpp"

#include <algorithm>
#include <spdlog/spdlog.h>

namespace balsa::visualization::vulkan {

// ── Constructor / Destructor ─────────────────────────────────────────

MeshScene::MeshScene()
  : _scene_root("Root") {
    // Create a camera Object as a child of the root.
    auto &cam_obj = _scene_root.add_child("Camera");
    _camera_object = &cam_obj;
    _camera = &cam_obj.emplace_feature<scene_graph::Camera>();
}

MeshScene::~MeshScene() {
    release_vulkan_resources();
}

// ── SceneBase overrides ──────────────────────────────────────────────

void MeshScene::initialize(Film &film) {
    SceneBase::initialize(film);
    _film = &film;

    _pipeline_manager.init(film);
    _initialized = true;

    // Init any VulkanMeshDrawables that were added before initialize().
    for (auto *drawable : _drawable_group) {
        auto *vmd = dynamic_cast<VulkanMeshDrawable *>(drawable);
        if (vmd && !vmd->is_initialized()) {
            vmd->init(film);
        }
    }

    spdlog::info("MeshScene initialized with {} drawable(s)", _drawable_group.size());
}

void MeshScene::draw(Film &film) {
    if (!_initialized) return;

    for (auto *drawable : _drawable_group) {
        auto *vmd = dynamic_cast<VulkanMeshDrawable *>(drawable);
        if (!vmd) continue;
        vmd->draw(*_camera, film);
    }
}

void MeshScene::release_vulkan_resources() {
    if (!_initialized) return;

    spdlog::debug("MeshScene: releasing Vulkan resources ({} drawables)", _drawable_group.size());

    // Release all VulkanMeshDrawables first (they reference pipeline
    // manager's descriptor pool).
    for (auto *drawable : _drawable_group) {
        auto *vmd = dynamic_cast<VulkanMeshDrawable *>(drawable);
        if (vmd) vmd->release();
    }

    // Then release the pipeline manager (destroys pool, layouts, cached
    // pipelines).
    _pipeline_manager.release();

    _film = nullptr;
    _initialized = false;
}

// ── Mesh management ──────────────────────────────────────────────────

scene_graph::Object &MeshScene::add_mesh(const std::string &name) {
    auto &obj = _scene_root.add_child(name);
    obj.emplace_feature<scene_graph::MeshData>();
    auto &vmd = obj.emplace_feature<VulkanMeshDrawable>(
      _drawable_group, _pipeline_manager);

    // If the scene is already initialized, init the drawable now.
    if (_initialized && _film) {
        vmd.init(*_film);
    }

    return obj;
}

bool MeshScene::remove_mesh(const scene_graph::Object *obj) {
    if (!obj) return false;

    auto &children = _scene_root.children();
    auto it = std::find_if(children.begin(), children.end(), [obj](const auto &p) { return p.get() == obj; });
    if (it == children.end()) return false;

    // Release GPU resources before removing from scene graph.
    auto *vmd = (*it)->find_feature<VulkanMeshDrawable>();
    if (vmd) vmd->release();

    // Remove from the scene graph.  The Object's destructor will
    // unregister its VulkanMeshDrawable from the DrawableGroup.
    // We need to use detach via the object itself.
    // Since children are owned by root, we need to remove from root's
    // children list.  Object doesn't expose erase-by-iterator, so
    // we use detach() on the child itself.
    auto *mutable_obj = const_cast<scene_graph::Object *>(obj);
    auto detached = mutable_obj->detach();
    // detached goes out of scope and is destroyed.

    return true;
}

std::size_t MeshScene::mesh_count() const {
    std::size_t count = 0;
    for (auto &child : _scene_root.children()) {
        if (child->find_feature<scene_graph::MeshData>()) {
            ++count;
        }
    }
    return count;
}

scene_graph::Object *MeshScene::mesh_object(std::size_t index) {
    std::size_t count = 0;
    for (auto &child : _scene_root.children()) {
        if (child->find_feature<scene_graph::MeshData>()) {
            if (count == index) return child.get();
            ++count;
        }
    }
    return nullptr;
}

const scene_graph::Object *MeshScene::mesh_object(std::size_t index) const {
    std::size_t count = 0;
    for (auto &child : _scene_root.children()) {
        if (child->find_feature<scene_graph::MeshData>()) {
            if (count == index) return child.get();
            ++count;
        }
    }
    return nullptr;
}

// ── Camera helpers ───────────────────────────────────────────────────

void MeshScene::look_at(const scene_graph::Vec3f &eye,
                        const scene_graph::Vec3f &center,
                        const scene_graph::Vec3f &up) {
    // The Camera derives view_matrix as inverse(world_transform).
    // lookAt() returns a view matrix, so we set the camera Object's
    // local transform to the *inverse* of the lookAt result, which
    // is the camera's world-space pose.
    //
    // However, since the camera is a direct child of root (which has
    // identity transform), local_transform == world_transform.  So:
    //   view = lookAt(eye, center, up)
    //   camera.local_transform = inverse(view)
    //
    // But Camera::view_matrix() already returns inverse(world_transform),
    // so setting local_transform = inverse(lookAt(...)) gives us
    // view_matrix() == lookAt(...).  That's correct.
    auto view = glm_compat::look_at(eye, center, up);
    auto cam_pose = glm_compat::inverse(view);
    _camera_object->set_local_transform(cam_pose);
}

void MeshScene::set_perspective(float fov_y, float aspect, float near, float far) {
    _camera->set_perspective(fov_y, aspect, near, far);
}

void MeshScene::update_aspect(float aspect) {
    _camera->update_aspect(aspect);
}

}// namespace balsa::visualization::vulkan
