#include "balsa/visualization/vulkan/mesh_scene.hpp"
#include "balsa/visualization/vulkan/vulkan_mesh_drawable.hpp"
#include "balsa/visualization/vulkan/film.hpp"
#include "balsa/scene_graph/Light.hpp"

#include <zipper/transform/view.hpp>
#include <algorithm>
#include <spdlog/spdlog.h>

namespace balsa::visualization::vulkan {

// ── Constructor / Destructor ─────────────────────────────────────────

MeshScene::MeshScene(const shaders::ShaderLibrary &library)
  : _scene_root("Root"), _pipeline_manager(library) {
    // Create a camera Object as a child of the root.
    auto &cam_obj = _scene_root.add_child("Camera");
    cam_obj.permanent = true;// internal fixture — hidden from UI, cannot be deleted
    _default_camera_object = &cam_obj;
    _active_camera_object = &cam_obj;
    _default_camera = &cam_obj.emplace_feature<scene_graph::Camera>();

    // Attach a default headlight to the camera Object.
    // Direction (0, 0, 1) in camera local space = shining along the
    // view direction, automatically tracking camera orientation.
    _headlight = &cam_obj.emplace_feature<scene_graph::Light>();
}

MeshScene::~MeshScene() {
    release_vulkan_resources();
}

// ── SceneBase overrides ──────────────────────────────────────────────

void MeshScene::initialize(Film &film) {
    SceneBase::initialize(film);
    _film = &film;

    // Each drawable allocates concurrent_frame_count() descriptor sets
    // (one per frame-in-flight).  Scale the pool accordingly.
    const uint32_t max_sets = 32 * static_cast<uint32_t>(film.concurrent_frame_count());
    _pipeline_manager.init(film, max_sets);
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

    // Resolve scene-level lighting once per frame.
    auto light_state = resolve_scene_lights();

    // Use the active camera for rendering.
    auto *active_cam = _active_camera_object->find_feature<scene_graph::Camera>();
    if (!active_cam) active_cam = _default_camera;

    for (auto *drawable : _drawable_group) {
        auto *vmd = dynamic_cast<VulkanMeshDrawable *>(drawable);
        if (!vmd) continue;
        vmd->set_scene_light_state(light_state);
        vmd->draw(*active_cam, film);
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

scene_graph::Object &
    MeshScene::add_mesh(const std::string &name,
                        std::shared_ptr<scene_graph::MeshGeometry> geometry) {
    if (!geometry) geometry = std::make_shared<scene_graph::MeshGeometry>();
    auto &obj = _scene_root.add_child(name);
    obj.emplace_feature<scene_graph::MeshInstance>(std::move(geometry));
    obj.emplace_feature<scene_graph::BasicShading>();
    auto &vmd = obj.emplace_feature<VulkanMeshDrawable>(
      _drawable_group, _pipeline_manager);

    // If the scene is already initialized, init the drawable now.
    if (_initialized && _film) {
        vmd.init(*_film);
    }

    return obj;
}

scene_graph::Object &MeshScene::add_mesh(const std::string &name,
                                         scene_graph::MeshGeometry geometry) {
    return add_mesh(
        name, std::make_shared<scene_graph::MeshGeometry>(std::move(geometry)));
}

void MeshScene::ensure_drawable(scene_graph::Object &obj) {
    // If the Object shows a mesh but has no VulkanMeshDrawable, add one.
    if (!obj.find_feature<scene_graph::MeshInstance>()) return;
    if (obj.find_feature<VulkanMeshDrawable>()) return;
    if (!obj.find_feature<scene_graph::BasicShading>()) {
        obj.emplace_feature<scene_graph::BasicShading>();
    }

    auto &vmd = obj.emplace_feature<VulkanMeshDrawable>(
      _drawable_group, _pipeline_manager);

    if (_initialized && _film) {
        vmd.init(*_film);
    }
}

bool MeshScene::remove_mesh(const scene_graph::Object *obj) {
    if (!obj) return false;

    auto &children = _scene_root.children();
    auto it = std::find_if(children.begin(), children.end(), [obj](const auto &p) { return p.get() == obj; });
    if (it == children.end()) return false;

    // Wait for the GPU to finish all in-flight command buffers before
    // releasing resources.  Without this, removing a mesh mid-frame
    // (e.g. when the BVH overlay rebuilds during ImGui interaction)
    // destroys buffers still referenced by queued draw commands,
    // causing a VK_ERROR_DEVICE_LOST.
    if (_initialized && _film) {
        _film->device().waitIdle();
    }

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
        if (child->find_feature<scene_graph::MeshInstance>()) {
            ++count;
        }
    }
    return count;
}

scene_graph::Object *MeshScene::mesh_object(std::size_t index) {
    std::size_t count = 0;
    for (auto &child : _scene_root.children()) {
        if (child->find_feature<scene_graph::MeshInstance>()) {
            if (count == index) return child.get();
            ++count;
        }
    }
    return nullptr;
}

const scene_graph::Object *MeshScene::mesh_object(std::size_t index) const {
    std::size_t count = 0;
    for (auto &child : _scene_root.children()) {
        if (child->find_feature<scene_graph::MeshInstance>()) {
            if (count == index) return child.get();
            ++count;
        }
    }
    return nullptr;
}

// ── Camera accessors ────────────────────────────────────────────────

scene_graph::Camera &MeshScene::camera() {
    auto *cam = _active_camera_object->find_feature<scene_graph::Camera>();
    return cam ? *cam : *_default_camera;
}

const scene_graph::Camera &MeshScene::camera() const {
    auto *cam = _active_camera_object->find_feature<scene_graph::Camera>();
    return cam ? *cam : *_default_camera;
}

scene_graph::Object &MeshScene::camera_object() {
    return *_active_camera_object;
}

const scene_graph::Object &MeshScene::camera_object() const {
    return *_active_camera_object;
}

void MeshScene::set_active_camera(scene_graph::Object *cam_obj) {
    if (!cam_obj) {
        _active_camera_object = _default_camera_object;
        return;
    }
    auto *cam = cam_obj->find_feature<scene_graph::Camera>();
    if (!cam) {
        spdlog::warn("set_active_camera: Object '{}' has no Camera feature", cam_obj->name);
        return;
    }
    _active_camera_object = cam_obj;
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
    auto view = ::zipper::transform::look_at(eye, center, up);
    auto inv_view = view.inverse();
    _default_camera_object->set_from_transform(
      scene_graph::AffineTransformf{ inv_view.to_matrix() });
}

void MeshScene::set_perspective(float fov_y, float aspect, float near, float far) {
    _default_camera->set_perspective(fov_y, aspect, near, far);
}

void MeshScene::update_aspect(float aspect) {
    _default_camera->update_aspect(aspect);
}

// ── Scene lighting ──────────────────────────────────────────────────

// Helper: recursively search the scene graph for the first enabled
// Light feature.  Returns {Light*, Object*} or {nullptr, nullptr}.
static std::pair<const scene_graph::Light *, const scene_graph::Object *>
  find_first_light(const scene_graph::Object &obj) {
    if (auto *light = obj.find_feature<scene_graph::Light>()) {
        if (light->enabled) {
            return { light, &obj };
        }
    }
    for (auto &child : obj.children()) {
        auto result = find_first_light(*child);
        if (result.first) return result;
    }
    return { nullptr, nullptr };
}

ResolvedLightState MeshScene::resolve_scene_lights() const {
    ResolvedLightState state;

    auto [light, obj] = find_first_light(_scene_root);
    if (!light || !obj) {
        // No lights in the scene — use defaults.
        return state;
    }

    // Transform the light direction from local space to world space.
    // The direction is a vector (not a point), so we use the linear
    // part (upper-left 3x3) of the world transform, ignoring translation.
    auto world_xf = obj->world_transform();
    scene_graph::Rotation3f lin(world_xf.linear());
    auto world_dir = (lin * light->direction).normalized();

    state.light_dir = world_dir;
    state.light_color = light->color;

    return state;
}

}// namespace balsa::visualization::vulkan
