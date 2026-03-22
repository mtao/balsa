#if !defined(BALSA_VISUALIZATION_VULKAN_VULKAN_MESH_DRAWABLE_HPP)
#define BALSA_VISUALIZATION_VULKAN_VULKAN_MESH_DRAWABLE_HPP

#include <cstdint>
#include <vulkan/vulkan.hpp>

#include "balsa/visualization/vulkan/buffer.hpp"
#include "balsa/visualization/vulkan/drawable.hpp"
#include "balsa/visualization/vulkan/mesh_buffers.hpp"
#include "balsa/visualization/vulkan/mesh_render_state.hpp"

namespace balsa::visualization::vulkan {

class Film;
class MeshPipelineManager;

// ── VulkanMeshDrawable ──────────────────────────────────────────────
//
// Scene-graph–aware bridge feature that connects a MeshData feature
// (CPU-side mesh geometry) to the Vulkan rendering pipeline.
//
// Lives as a feature on the same Object as a MeshData feature.  Owns
// the GPU resources (MeshBuffers, UBO buffers, descriptor set) and
// syncs from MeshData using version tracking.
//
// The model matrix comes from Object::world_transform(), not from a
// standalone glm::mat4 field (unlike the legacy MeshDrawable).
//
// Lifecycle:
//   1. Attach to an Object that already has a MeshData feature
//   2. Call init(film, pipeline_manager) once Vulkan is ready
//   3. Each frame: draw(camera, film) — syncs data if dirty, updates
//      UBOs, and issues draw commands
//   4. On teardown: release() or let destructor handle it
//
// Thread-safety: NOT thread-safe — call only from the rendering thread.

class VulkanMeshDrawable : public VulkanDrawable {
  public:
    // Construct with the DrawableGroup this drawable belongs to, and
    // a reference to the MeshPipelineManager that owns descriptor
    // layouts / pipeline cache.
    VulkanMeshDrawable(scene_graph::DrawableGroup &group,
                       MeshPipelineManager &manager);

    ~VulkanMeshDrawable() override;

    // Non-copyable (base class already), non-movable (registered).
    VulkanMeshDrawable(VulkanMeshDrawable &&) = delete;
    VulkanMeshDrawable &operator=(VulkanMeshDrawable &&) = delete;

    // ── Lifecycle ───────────────────────────────────────────────────

    // Allocate UBO buffers and a descriptor set.
    // Must be called once after the Film is available (Vulkan init).
    void init(Film &film);

    // Release all GPU resources.  Safe to call multiple times.
    void release();

    bool is_initialized() const { return _initialized; }

    // ── VulkanDrawable interface ────────────────────────────────────

    // Draw this mesh with the given camera.  Syncs from MeshData if
    // dirty, updates UBOs with the camera's view/projection and the
    // Object's world transform, then issues Vulkan draw commands.
    void draw(const scene_graph::Camera &cam, Film &film) override;

    // ── Scene lighting ──────────────────────────────────────────────

    // Set the resolved scene-level light state for this frame.
    // Called by MeshScene::draw() before each drawable's draw().
    // If the mesh's MeshRenderState::use_scene_lights is true,
    // update_ubos() will pack MaterialUBO from this state instead
    // of from the per-mesh lighting fields.
    void set_scene_light_state(const ResolvedLightState &state) {
        _scene_light_state = state;
    }

  private:
    // Sync GPU buffers from the sibling MeshData feature if its
    // version has changed since our last sync.
    void sync_from_mesh_data(Film &film);

    // Upload TransformUBO and MaterialUBO.
    void update_ubos(const scene_graph::Camera &cam);

    // Issue the actual Vulkan draw commands (bind pipeline, buffers,
    // dispatch draw).
    void record_draw_commands(Film &film);

    MeshPipelineManager *_manager;
    MeshBuffers _buffers;

    VulkanBuffer _transform_ubo;
    VulkanBuffer _material_ubo;
    vk::DescriptorSet _descriptor_set;

    ResolvedLightState _scene_light_state;
    uint64_t _synced_version = 0;
    bool _initialized = false;
};

}// namespace balsa::visualization::vulkan

#endif
