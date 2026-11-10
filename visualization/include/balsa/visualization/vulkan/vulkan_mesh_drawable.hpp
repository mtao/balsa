#pragma once
#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include <vulkan/vulkan.hpp>

#include "balsa/scene_graph/GeometryView.hpp"
#include "balsa/scene_graph/Material.hpp"
#include "balsa/scene_graph/MeshRenderer.hpp"
#include "balsa/scene_graph/MeshRole.hpp"
#include "balsa/visualization/vulkan/buffer.hpp"
#include "balsa/visualization/vulkan/drawable.hpp"
#include "balsa/visualization/vulkan/mesh_buffers.hpp"
#include "balsa/visualization/vulkan/mesh_pipeline.hpp"
#include "balsa/visualization/vulkan/mesh_render_state.hpp"

namespace balsa::scene_graph {
class BasicShading;
class MeshGeometry;
class MeshRenderer;
struct MeshRenderLayer;
class MeshLayerContext;
} // namespace balsa::scene_graph

namespace balsa::visualization::vulkan {

class Film;
class MeshPipelineManager;

// ── VulkanMeshDrawable ──────────────────────────────────────────────
//
// Scene-graph–aware bridge feature that draws the MeshGeometry an Object
// shows (through its MeshInstance) with its BasicShading through the
// Vulkan rendering pipeline.
//
// Lives as a feature on the same Object as the MeshInstance and
// BasicShading features.  Owns the GPU resources (MeshBuffers, UBO
// buffers, descriptor set).  Index buffers follow the geometry's
// version; each render layer draws the attributes its roles resolve to
// (BasicShading::layer_attribute), uploaded once per attribute.
//
// The model matrix comes from Object::world_transform(), not from a
// standalone glm::mat4 field (unlike the legacy MeshDrawable).
//
// Lifecycle:
//   1. Attach to an Object that has a MeshInstance and BasicShading
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

    // Draw this mesh with the given camera.  Re-uploads buffers if the
    // geometry or shading changed, updates UBOs with the camera's view/projection and the
    // Object's world transform, then issues Vulkan draw commands.
    void draw(const scene_graph::Camera &cam, Film &film) override;

    // ── Scene lighting ──────────────────────────────────────────────

    // Set the resolved scene-level light state for this frame.
    // Called by MeshScene::draw() before each drawable's draw().
    void set_scene_light_state(const ResolvedLightState &state) {
        _scene_light_state = state;
    }

  private:
    // GPU buffers for one view, re-synced when its geometry changes.
    struct ViewBuffers {
        explicit ViewBuffers(scene_graph::GeometryView view) : view(view) {}
        scene_graph::GeometryView view;
        // Owned so a rebuilt view cannot reuse its address.
        std::shared_ptr<const scene_graph::MeshGeometry> geometry;
        uint64_t version = 0;
        MeshBuffers buffers;
    };

    // Synced buffers for the layer's view; nullptr if it is empty.
    auto buffers_for(Film &film,
                     const scene_graph::BasicShading &shading,
                     const scene_graph::MeshRenderLayer &layer) -> MeshBuffers *;

    // Upload the frame's TransformUBO and SceneLightingUBO.
    void update_scene_ubos(const scene_graph::Camera &cam, Film &film);

    // Record each enabled layer's draw: its program's pipeline, its vertex
    // streams, and its material at the layer slot's offset.
    void record_draw_commands(Film &film);

    // The layer slot's material set for this frame, (re)written for
    // `program` and `textures`.
    auto material_set(Film &film,
                      uint32_t layer_slot,
                      const shaders::ShaderProgram &program,
                      const scene_graph::MeshMaterial &material) -> vk::DescriptorSet;

    MeshPipelineManager *_manager;
    // One per view drawn.
    std::vector<std::unique_ptr<ViewBuffers>> _view_buffers;

    // Per frame slot, so the CPU never writes what the GPU may still read.
    struct FrameResources {
        VulkanBuffer transforms;
        VulkanBuffer lighting;
        // k_max_material_layers blocks, _material_stride apart.
        VulkanBuffer materials;
        vk::DescriptorSet scene_set;
        struct MaterialSet {
            vk::DescriptorSet set;
            std::string program; // id; empty if unallocated
            std::vector<const scene_graph::Texture *> textures{};
        };
        std::array<MaterialSet, k_max_material_layers> material_sets;
    };
    std::vector<FrameResources> _frames;
    vk::DeviceSize _material_stride = 0;

    ResolvedLightState _scene_light_state;
    bool _initialized = false;
};

}// namespace balsa::visualization::vulkan
