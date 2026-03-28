#if !defined(BALSA_VISUALIZATION_VULKAN_VULKAN_IMAGE_DRAWABLE_HPP)
#define BALSA_VISUALIZATION_VULKAN_VULKAN_IMAGE_DRAWABLE_HPP

#include <cstdint>
#include <vector>
#include <vulkan/vulkan.hpp>

#include "balsa/visualization/vulkan/buffer.hpp"
#include "balsa/visualization/vulkan/drawable.hpp"
#include "balsa/visualization/vulkan/texture.hpp"

namespace balsa::visualization::vulkan {

class Film;
class ImagePipelineManager;

// ── VulkanImageDrawable ─────────────────────────────────────────────
//
// Scene-graph-aware bridge feature that connects an ImageData feature
// (CPU-side pixel buffer) to the Vulkan image rendering pipeline.
//
// Lives as a feature on the same Object as an ImageData feature.  Owns
// the GPU resources (VulkanTexture, UBO buffers, descriptor sets) and
// syncs from ImageData using version tracking.
//
// Lifecycle:
//   1. Attach to an Object that already has an ImageData feature
//   2. Call init(film) once Vulkan is ready
//   3. Each frame: draw(camera, film) — syncs texture if dirty,
//      updates UBOs, and issues a fullscreen triangle draw
//   4. On teardown: release() or let destructor handle it
//
// Thread-safety: NOT thread-safe — call only from the rendering thread.

class VulkanImageDrawable : public VulkanDrawable {
  public:
    // Construct with the DrawableGroup this drawable belongs to, and
    // a reference to the ImagePipelineManager that owns descriptor
    // layouts / pipeline cache.
    VulkanImageDrawable(scene_graph::DrawableGroup &group,
                        ImagePipelineManager &manager);

    ~VulkanImageDrawable() override;

    // Non-copyable (base class already), non-movable (registered).
    VulkanImageDrawable(VulkanImageDrawable &&) = delete;
    VulkanImageDrawable &operator=(VulkanImageDrawable &&) = delete;

    // ── Lifecycle ───────────────────────────────────────────────────

    // Allocate UBO buffers and descriptor sets (one per concurrent
    // frame slot).  Must be called once after the Film is available.
    void init(Film &film);

    // Release all GPU resources.  Safe to call multiple times.
    void release();

    bool is_initialized() const { return _initialized; }

    // ── VulkanDrawable interface ────────────────────────────────────

    // Draw this image with the given camera.  Syncs from ImageData if
    // dirty, updates UBOs with the MVP and tone-mapping params, then
    // issues a fullscreen triangle draw.
    void draw(const scene_graph::Camera &cam, Film &film) override;

    // ── MVP override ────────────────────────────────────────────────

    // Set a custom MVP matrix for 2D viewing (orthographic pan/zoom).
    // When set, this overrides the camera-derived MVP.
    void set_mvp_override(const scene_graph::Mat4f &mvp) {
        _mvp_override = mvp;
        _has_mvp_override = true;
    }
    void clear_mvp_override() { _has_mvp_override = false; }

  private:
    // Sync GPU texture from the sibling ImageData feature if its
    // version has changed since our last sync.
    void sync_from_image_data(Film &film);

    // Upload TransformUBO (MVP) and ImageParamsUBO (tone mapping).
    void update_ubos(const scene_graph::Camera &cam, Film &film);

    // Issue fullscreen triangle draw commands.
    void record_draw_commands(Film &film);

    ImagePipelineManager *_manager;
    VulkanTexture _texture;

    // Per-frame UBO buffers and descriptor sets — one per concurrent
    // frame slot.  Sized by film.concurrent_frame_count() in init().
    std::vector<VulkanBuffer> _transform_ubos;
    std::vector<VulkanBuffer> _params_ubos;
    std::vector<vk::DescriptorSet> _descriptor_sets;

    scene_graph::Mat4f _mvp_override;
    bool _has_mvp_override = false;

    uint64_t _synced_version = 0;
    bool _initialized = false;
};

} // namespace balsa::visualization::vulkan

#endif
