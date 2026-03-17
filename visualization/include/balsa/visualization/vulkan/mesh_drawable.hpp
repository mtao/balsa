#if !defined(BALSA_VISUALIZATION_VULKAN_MESH_DRAWABLE_HPP)
#define BALSA_VISUALIZATION_VULKAN_MESH_DRAWABLE_HPP

#include <string>
#include <vulkan/vulkan.hpp>
#include <glm/mat4x4.hpp>

#include "balsa/visualization/vulkan/buffer.hpp"
#include "balsa/visualization/vulkan/mesh_buffers.hpp"
#include "balsa/visualization/vulkan/mesh_render_state.hpp"

namespace balsa::visualization::vulkan {

class Film;
class MeshPipelineManager;

// ── MeshDrawable ─────────────────────────────────────────────────────
//
// Represents a single drawable mesh object.  Owns the GPU mesh data
// (MeshBuffers), per-object rendering state, per-object UBO buffers
// (TransformUBO, MaterialUBO), and a descriptor set.
//
// Standalone class (not inheriting from the scene graph's Drawable /
// AbstractFeature) — the scene graph feature API doesn't yet support
// public iteration, so MeshScene manages MeshDrawables directly.
// Can be refactored into a proper Feature later.
//
// Thread-safety: NOT thread-safe — call only from the rendering thread.

class MeshDrawable {
  public:
    MeshDrawable() = default;
    ~MeshDrawable();

    // Non-copyable (owns Vulkan resources), movable
    MeshDrawable(const MeshDrawable &) = delete;
    MeshDrawable &operator=(const MeshDrawable &) = delete;
    MeshDrawable(MeshDrawable &&) noexcept;
    MeshDrawable &operator=(MeshDrawable &&) noexcept;

    // ── Public state (readable/writable by UI panels) ───────────────

    // Per-object rendering parameters.
    MeshRenderState render_state;

    // Model-to-world transform.
    glm::mat4 model_matrix = glm::mat4(1.0f);

    // Display name (for scene tree / UI).
    std::string name = "Mesh";

    // Visibility toggle.
    bool visible = true;

    // ── GPU mesh data ───────────────────────────────────────────────

    // The vertex/index buffers.  Populated by the user before drawing.
    MeshBuffers &buffers() { return _buffers; }
    const MeshBuffers &buffers() const { return _buffers; }

    // ── Lifecycle ───────────────────────────────────────────────────

    // Allocate UBO buffers and a descriptor set.
    // Must be called once after buffers are uploaded, before the first draw.
    void init(Film &film, MeshPipelineManager &manager);

    // Update the TransformUBO and MaterialUBO from the current
    // model_matrix, render_state, and the given view/projection.
    void update_ubos(const glm::mat4 &view, const glm::mat4 &projection);

    // Bind the pipeline matching the current state, bind descriptor
    // set and vertex/index buffers, then issue the draw command.
    void draw(Film &film, MeshPipelineManager &manager);

    // Release all GPU resources (UBOs, descriptor set, mesh buffers).
    void release();

    bool is_initialized() const { return _initialized; }

  private:
    MeshBuffers _buffers;

    VulkanBuffer _transform_ubo;
    VulkanBuffer _material_ubo;
    vk::DescriptorSet _descriptor_set;

    bool _initialized = false;
};

}// namespace balsa::visualization::vulkan

#endif
