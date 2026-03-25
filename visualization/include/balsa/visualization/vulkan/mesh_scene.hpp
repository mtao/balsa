#if !defined(BALSA_VISUALIZATION_VULKAN_MESH_SCENE_HPP)
#define BALSA_VISUALIZATION_VULKAN_MESH_SCENE_HPP

#include <memory>
#include <string>
#include <vector>

#include "balsa/scene_graph/Object.hpp"
#include "balsa/scene_graph/Camera.hpp"
#include "balsa/scene_graph/DrawableGroup.hpp"
#include "balsa/scene_graph/MeshData.hpp"
#include "balsa/scene_graph/Light.hpp"
#include "balsa/visualization/vulkan/scene_base.hpp"
#include "balsa/visualization/vulkan/mesh_pipeline.hpp"
#include "balsa/visualization/vulkan/mesh_render_state.hpp"

namespace balsa::visualization::vulkan {

class VulkanMeshDrawable;

// ── MeshScene ────────────────────────────────────────────────────────
//
// SceneBase subclass that uses a scene graph for camera and mesh
// management.  Owns:
//
//   - A root scene_graph::Object (the scene graph root)
//   - A camera Object (child of root) with a Camera feature
//   - A DrawableGroup (flat registry of all VulkanMeshDrawables)
//   - A MeshPipelineManager (shared by all drawables)
//
// Lifecycle:
//   1. Construct MeshScene
//   2. Add meshes via add_mesh() — creates Object + MeshData +
//      VulkanMeshDrawable in the scene graph
//   3. Call initialize(film) — creates pipeline manager, inits all
//      VulkanMeshDrawables
//   4. Each frame: Window calls begin_render_pass → draw → end_render_pass
//   5. On teardown: release_vulkan_resources() destroys everything
//
// Thread-safety: NOT thread-safe — call only from the rendering thread.

class MeshScene : public SceneBase {
  public:
    MeshScene();
    ~MeshScene() override;

    // Non-copyable, non-movable (holds Vulkan resources + raw Film pointer)
    MeshScene(const MeshScene &) = delete;
    MeshScene &operator=(const MeshScene &) = delete;
    MeshScene(MeshScene &&) = delete;
    MeshScene &operator=(MeshScene &&) = delete;

    // ── SceneBase overrides ─────────────────────────────────────────

    // Initialize the pipeline manager and all VulkanMeshDrawables.
    void initialize(Film &film) override;

    // Apply deferred BVH overlay updates before the render pass.
    void begin_render_pass(Film &film) override;

    // Draw all visible drawables.
    void draw(Film &film) override;

    // Destroy all Vulkan resources (pipeline manager, drawables, UBOs).
    // Idempotent — safe to call from both the renderer and destructor.
    void release_vulkan_resources() override;

    // ── Mesh management (scene graph API) ───────────────────────────

    // Add a mesh to the scene.  Creates a child Object under the root
    // with MeshData and VulkanMeshDrawable features.  Returns the
    // Object so the caller can populate its MeshData and set transforms.
    //
    // If the scene is already initialized, the VulkanMeshDrawable is
    // immediately init'd with the Film.
    scene_graph::Object &add_mesh(const std::string &name = "Mesh");

    // Remove a mesh Object from the scene by pointer.  Releases its
    // VulkanMeshDrawable GPU resources and removes it from the scene
    // graph.  Returns true if found and removed.
    bool remove_mesh(const scene_graph::Object *obj);

    // Number of mesh Objects in the scene.
    std::size_t mesh_count() const;

    // Access mesh Objects by index (children of root that have MeshData).
    // Returns nullptr if index is out of range.
    scene_graph::Object *mesh_object(std::size_t index);
    const scene_graph::Object *mesh_object(std::size_t index) const;

    // ── Camera ──────────────────────────────────────────────────────

    // The active camera's Camera feature.
    scene_graph::Camera &camera();
    const scene_graph::Camera &camera() const;

    // The active camera Object (for reading position/orientation).
    scene_graph::Object &camera_object();
    const scene_graph::Object &camera_object() const;

    // The default (permanent, hidden) camera Object.
    scene_graph::Object &default_camera_object() { return *_default_camera_object; }
    const scene_graph::Object &default_camera_object() const { return *_default_camera_object; }

    // The active camera Object (may differ from the default).
    // Returns the default camera if no other camera has been activated.
    scene_graph::Object &active_camera_object() { return *_active_camera_object; }
    const scene_graph::Object &active_camera_object() const { return *_active_camera_object; }

    // Set the active camera.  Pass nullptr to revert to the default.
    // The Object must have a Camera feature.
    void set_active_camera(scene_graph::Object *cam_obj);

    // Returns true if the active camera is the default camera.
    bool is_default_camera_active() const { return _active_camera_object == _default_camera_object; }

    // Convenience: set camera position via look_at parameters.
    // Always operates on the **default** camera Object.
    void look_at(const scene_graph::Vec3f &eye,
                 const scene_graph::Vec3f &center,
                 const scene_graph::Vec3f &up);

    // Convenience: set perspective projection on the camera.
    void set_perspective(float fov_y, float aspect, float near, float far);

    // Convenience: update aspect ratio (e.g. on window resize).
    void update_aspect(float aspect);

    // ── Scene graph access ──────────────────────────────────────────

    scene_graph::Object &root() { return _scene_root; }
    const scene_graph::Object &root() const { return _scene_root; }

    scene_graph::DrawableGroup &drawable_group() { return _drawable_group; }
    const scene_graph::DrawableGroup &drawable_group() const { return _drawable_group; }

    // ── Drawable helpers ───────────────────────────────────────────

    // Ensure an Object has a VulkanMeshDrawable.  If the Object has a
    // MeshData feature but no VulkanMeshDrawable, one is emplaced and
    // initialized (if the scene is already initialized).  No-op if the
    // Object already has a VulkanMeshDrawable or has no MeshData.
    void ensure_drawable(scene_graph::Object &obj);

    // ── Pipeline manager access (for advanced use) ──────────────────

    MeshPipelineManager &pipeline_manager() { return _pipeline_manager; }
    const MeshPipelineManager &pipeline_manager() const { return _pipeline_manager; }

    // ── Scene lighting ──────────────────────────────────────────────

    // The default headlight (Light feature attached to the camera Object).
    // Direction (0,0,1) in camera local space means it shines along
    // the view direction, automatically tracking camera orientation.
    scene_graph::Light &headlight() { return *_headlight; }
    const scene_graph::Light &headlight() const { return *_headlight; }

    // Walk the scene graph and resolve all enabled Light features
    // into a single ResolvedLightState (world-space direction + params).
    // Currently uses the first enabled Light found.
    ResolvedLightState resolve_scene_lights() const;

  private:
    // Scene graph
    scene_graph::Object _scene_root;
    scene_graph::Object *_default_camera_object = nullptr;
    scene_graph::Object *_active_camera_object = nullptr;
    scene_graph::Camera *_default_camera = nullptr;
    scene_graph::Light *_headlight = nullptr;
    scene_graph::DrawableGroup _drawable_group;

    // Vulkan resources
    MeshPipelineManager _pipeline_manager;
    Film *_film = nullptr;
    bool _initialized = false;
};

}// namespace balsa::visualization::vulkan

#endif
