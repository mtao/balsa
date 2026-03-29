#if !defined(BALSA_VISUALIZATION_VULKAN_IMAGE_PIPELINE_HPP)
#define BALSA_VISUALIZATION_VULKAN_IMAGE_PIPELINE_HPP

#include <vulkan/vulkan.hpp>

#include "balsa/scene_graph/types.hpp"

namespace balsa::visualization::vulkan {

class Film;

// ── UBO structs for image rendering (must match GLSL, std140) ───────

// binding = 0 in image.vert
struct ImageTransformUBO {
    scene_graph::Mat4f mvp; // 64 bytes
};
static_assert(sizeof(ImageTransformUBO) == 64,
              "ImageTransformUBO must be 64 bytes");

// binding = 1 in image.frag
struct ImageParamsUBO {
    scene_graph::Vec4f
        tone_params; // x=exposure, y=gamma, z=channel_mode, w=unused
    scene_graph::Vec4f image_size; // x=width, y=height, z=1/width, w=1/height
};
static_assert(sizeof(ImageParamsUBO) == 32, "ImageParamsUBO must be 32 bytes");

// ── ImagePipelineManager ────────────────────────────────────────────
//
// Owns the descriptor set layout, pipeline layout, descriptor pool,
// and a single graphics pipeline for image (textured fullscreen
// triangle) rendering.
//
// Descriptor set layout:
//   binding 0: ImageTransformUBO  (uniform buffer, vertex stage)
//   binding 1: ImageParamsUBO     (uniform buffer, fragment stage)
//   binding 2: combined image sampler (fragment stage)
//
// The pipeline uses the fullscreen triangle technique (3 vertices,
// no vertex buffer) with the image.vert / image.frag shaders.
//
// Thread-safety: NOT thread-safe — call only from the rendering thread.

class ImagePipelineManager {
  public:
    ImagePipelineManager() = default;
    ~ImagePipelineManager();

    // Non-copyable, movable
    ImagePipelineManager(const ImagePipelineManager &) = delete;
    auto operator=(const ImagePipelineManager &) -> ImagePipelineManager & = delete;
    ImagePipelineManager(ImagePipelineManager &&) noexcept;
    auto operator=(ImagePipelineManager &&) noexcept -> ImagePipelineManager &;

    // Initialise with a Film reference.  Must be called before any
    // other method.
    auto init(Film &film, uint32_t max_descriptor_sets = 16) -> void;

    // ── Pipeline access ─────────────────────────────────────────────

    // Get (or lazily create) the pipeline.  The Film is queried for
    // render-pass / MSAA / depth-stencil info.
    auto get_or_create(Film &film) -> vk::Pipeline;

    // The shared pipeline layout.
    auto pipeline_layout() const -> vk::PipelineLayout { return _pipeline_layout; }

    // ── Descriptor sets ─────────────────────────────────────────────

    // Allocate a descriptor set from the internal pool.
    auto allocate_descriptor_set() -> vk::DescriptorSet;

    // Write buffer + image bindings into a descriptor set.
    //   binding 0 = ImageTransformUBO  (uniform buffer)
    //   binding 1 = ImageParamsUBO     (uniform buffer)
    //   binding 2 = combined image sampler
    auto write_descriptor_set(vk::DescriptorSet ds,
                              vk::Buffer transform_buffer,
                              vk::DeviceSize transform_size,
                              vk::Buffer params_buffer,
                              vk::DeviceSize params_size,
                              vk::ImageView image_view,
                              vk::Sampler sampler) -> void;

    // Free a descriptor set back to the pool.
    auto free_descriptor_set(vk::DescriptorSet ds) -> void;

    // ── Lifecycle ───────────────────────────────────────────────────

    // Destroy the cached pipeline (e.g. on swapchain recreation).
    auto invalidate_pipeline() -> void;

    // Destroy everything.  Safe to call multiple times.
    auto release() -> void;

    auto is_initialized() const -> bool { return _initialized; }

  private:
    auto create_pipeline() -> vk::Pipeline;

    auto create_descriptor_set_layout() -> void;
    auto create_pipeline_layout() -> void;
    auto create_descriptor_pool(uint32_t max_sets) -> void;

    vk::Device _device;
    Film *_film = nullptr;

    vk::DescriptorSetLayout _descriptor_set_layout;
    vk::PipelineLayout _pipeline_layout;
    vk::DescriptorPool _descriptor_pool;

    // Pipeline key: (render_pass, msaa_samples, depth_test).
    // For simplicity we store a single cached pipeline and
    // invalidate on render pass changes (same as swapchain recreate).
    vk::Pipeline _pipeline;
    uint64_t _cached_render_pass = 0;
    uint32_t _cached_msaa_samples = 0;
    bool _cached_depth_test = false;

    bool _initialized = false;
};

} // namespace balsa::visualization::vulkan

#endif
