#if !defined(BALSA_VISUALIZATION_VULKAN_MESH_PIPELINE_HPP)
#define BALSA_VISUALIZATION_VULKAN_MESH_PIPELINE_HPP

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>
#include <vulkan/vulkan.hpp>

#include "balsa/visualization/vulkan/mesh_render_state.hpp"

namespace balsa::visualization::vulkan {

class Film;
class MeshBuffers;

// ── MeshPipelineKey ──────────────────────────────────────────────────
//
// Captures every piece of state that can cause a distinct Vulkan
// graphics pipeline to be needed.  Pipelines are baked for a
// particular (render_pass, MSAA samples, topology, polygon mode,
// shader permutation) combination.

struct MeshPipelineKey {
    // Shader permutation state
    ShadingModel shading = ShadingModel::Phong;
    ColorSource color_source = ColorSource::Uniform;
    NormalSource normal_source = NormalSource::FromAttribute;
    RenderMode render_mode = RenderMode::Solid;

    // Colormap name — only meaningful when color_source == ScalarField.
    // Normalised to "" for other color sources so that keys compare equal
    // regardless of what the user last typed in the colormap text field.
    std::string colormap_name;

    // Vertex attribute flags (determines vertex input layout)
    bool has_normals = false;
    bool has_colors = false;
    bool has_scalars = false;

    // Rasterization state
    bool two_sided = true;// false = cull back faces

    // Render-pass-dependent state (queried from Film)
    uint32_t msaa_samples = 1;// underlying value of VkSampleCountFlagBits
    uint64_t render_pass = 0;// VkRenderPass cast to uint64_t for hashing
    bool depth_test = false;

    bool operator==(const MeshPipelineKey &) const = default;
};

struct MeshPipelineKeyHash {
    std::size_t operator()(const MeshPipelineKey &k) const noexcept;
};

// Build a MeshPipelineKey from the current render state, buffer
// contents, and film configuration.
MeshPipelineKey make_pipeline_key(const MeshRenderState &state,
                                  const MeshBuffers &buffers,
                                  Film &film);

// ── MeshPipelineManager ──────────────────────────────────────────────
//
// Owns the descriptor set layout, pipeline layout, descriptor pool,
// and a cache of compiled Vulkan graphics pipelines keyed by
// MeshPipelineKey.
//
// Non-template class (mesh viewer is always 3D).  Internally uses
// MeshShader<embedding_traits3F> for SPIR-V compilation.
//
// Thread-safety: NOT thread-safe — call only from the rendering thread.

class MeshPipelineManager {
  public:
    MeshPipelineManager() = default;
    ~MeshPipelineManager();

    // Non-copyable, movable
    MeshPipelineManager(const MeshPipelineManager &) = delete;
    MeshPipelineManager &operator=(const MeshPipelineManager &) = delete;
    MeshPipelineManager(MeshPipelineManager &&) noexcept;
    MeshPipelineManager &operator=(MeshPipelineManager &&) noexcept;

    // Initialise with a Film reference.  Must be called before any
    // other method.  Safe to call again after release() to re-init
    // for a new device/render-pass.
    void init(Film &film, uint32_t max_descriptor_sets = 32);

    // ── Pipeline access ─────────────────────────────────────────────

    // Get (or lazily create) the pipeline matching the given state and
    // buffer configuration.  The Film is queried for render-pass /
    // MSAA / depth-stencil info.
    vk::Pipeline get_or_create(const MeshRenderState &state,
                               const MeshBuffers &buffers,
                               Film &film);

    // The shared pipeline layout (all mesh pipelines use the same one).
    vk::PipelineLayout pipeline_layout() const { return _pipeline_layout; }

    // ── Descriptor sets ─────────────────────────────────────────────

    // Allocate a descriptor set from the internal pool.
    vk::DescriptorSet allocate_descriptor_set();

    // Write UBO buffer bindings into a descriptor set.
    //   binding 0 = TransformUBO   (vertex stage)
    //   binding 1 = MaterialUBO    (vertex + fragment stage)
    void write_descriptor_set(vk::DescriptorSet ds,
                              vk::Buffer transform_buffer,
                              vk::DeviceSize transform_size,
                              vk::Buffer material_buffer,
                              vk::DeviceSize material_size);

    // ── Lifecycle ───────────────────────────────────────────────────

    // Destroy all cached pipelines (e.g. on swapchain recreation when
    // the render pass changes).  Keeps the layout and pool alive.
    void invalidate_pipelines();

    // Destroy everything.  Safe to call multiple times.
    void release();

    bool is_initialized() const { return _initialized; }

  private:
    vk::Pipeline create_pipeline(const MeshPipelineKey &key);

    void create_descriptor_set_layout();
    void create_pipeline_layout();
    void create_descriptor_pool(uint32_t max_sets);

    vk::Device _device;
    Film *_film = nullptr;

    vk::DescriptorSetLayout _descriptor_set_layout;
    vk::PipelineLayout _pipeline_layout;
    vk::DescriptorPool _descriptor_pool;

    std::unordered_map<MeshPipelineKey, vk::Pipeline, MeshPipelineKeyHash> _cache;

    bool _initialized = false;
};

}// namespace balsa::visualization::vulkan

#endif
