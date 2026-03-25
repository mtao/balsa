#if !defined(BALSA_VISUALIZATION_VULKAN_VULKAN_MESH_DRAWABLE_HPP)
#define BALSA_VISUALIZATION_VULKAN_VULKAN_MESH_DRAWABLE_HPP

#include <cstdint>
#include <vector>
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
    // upload_material_ubo_for_layer() packs MaterialUBO using
    // this state for light direction and per-mesh MaterialProperties
    // for material response (ambient, specular, shininess).
    void set_scene_light_state(const ResolvedLightState &state) {
        _scene_light_state = state;
    }

  private:
    // Sync GPU buffers from the sibling MeshData feature if its
    // version has changed since our last sync.
    void sync_from_mesh_data(Film &film);

    // Upload TransformUBO (model/view/projection/normal_matrix).
    // Called once per frame.  Uses film.current_frame() to select
    // the correct per-frame UBO buffer.
    void update_transform_ubo(const scene_graph::Camera &cam, Film &film);

    // Pack and upload MaterialUBO for a specific layer slot, filling
    // layer_color from the layer's colour and material response
    // from _scene_light_state + per-mesh MaterialProperties.
    // layer_slot: 0 = solid, 1 = wireframe, 2 = points.
    // Uses film.current_frame() to select the correct per-frame UBO.
    void upload_material_ubo_for_layer(Film &film,
                                       uint32_t layer_slot,
                                       const float layer_color[4],
                                       float point_size,
                                       const MeshRenderState &rs,
                                       const float *wireframe_color_override = nullptr);

    // Bind the descriptor set with dynamic offset for the given layer.
    // Uses film.current_frame() to select the correct per-frame set.
    void bind_descriptor_set_for_layer(Film &film,
                                       vk::CommandBuffer cb,
                                       uint32_t layer_slot);

    // Issue multi-layer Vulkan draw commands (solid, wireframe,
    // points).  Each enabled layer uploads to its own material UBO
    // slot and binds with a dynamic offset, so all layers read
    // independent data even though the command buffer is deferred.
    void record_draw_commands(Film &film);

    MeshPipelineManager *_manager;
    MeshBuffers _buffers;

    // Per-frame UBO buffers and descriptor sets — one per concurrent
    // frame slot — so the CPU can upload data for the current frame
    // without clobbering data the GPU is still reading from a prior
    // frame.  Sized by film.concurrent_frame_count() in init().
    std::vector<VulkanBuffer> _transform_ubos;
    std::vector<VulkanBuffer> _material_ubos;// each sized for k_max_material_layers aligned slots
    std::vector<vk::DescriptorSet> _descriptor_sets;
    vk::DeviceSize _material_ubo_stride = 0;// aligned size per layer slot

    ResolvedLightState _scene_light_state;
    uint64_t _synced_version = 0;
    bool _initialized = false;
};

}// namespace balsa::visualization::vulkan

#endif
