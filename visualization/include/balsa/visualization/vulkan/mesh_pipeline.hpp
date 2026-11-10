#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <unordered_map>
#include <vulkan/vulkan.hpp>

#include "balsa/visualization/shaders/ShaderProgram.hpp"
#include "balsa/visualization/shaders/program_shader.hpp"
#include "balsa/visualization/shaders/shader_library.hpp"
#include "balsa/visualization/vulkan/DescriptorAllocator.hpp"
#include "balsa/visualization/vulkan/mesh_render_state.hpp"
#include "balsa/visualization/vulkan/texture_cache.hpp"

namespace balsa::visualization::vulkan {

class Film;

// Vertex streams, by shader location (VertexStream::location).
inline constexpr std::size_t max_vertex_streams = 8;

// Everything that makes a mesh pipeline distinct.
struct MeshPipelineKey {
    std::string program; // ShaderProgram id
    shaders::ProgramVariant variant;
    // Bound streams' formats; eUndefined where none is bound.
    std::array<vk::Format, max_vertex_streams> stream_formats{};
    vk::PrimitiveTopology topology = vk::PrimitiveTopology::eTriangleList;
    CullMode cull_mode = CullMode::None;
    bool depth_write = true;
    // From the Film's render pass.
    uint32_t msaa_samples = 1;
    uint64_t render_pass = 0;
    bool depth_test = false;

    auto operator==(const MeshPipelineKey &) const -> bool = default;
};

struct MeshPipelineKeyHash {
    auto operator()(const MeshPipelineKey &key) const noexcept -> std::size_t;
};

// Pipelines and descriptor sets of shader programs (shaders::ShaderProgram),
// shared by the drawables of a scene.
//
// Descriptor set 0 ("scene") holds the transforms and the scene lighting;
// set 1 ("material") holds a program's material block, bound with a dynamic
// offset, and its textures.  Pipeline layouts differ per program only in
// set 1, so set 0 can be bound once for any of them.
class MeshPipelineManager {
  public:
    explicit MeshPipelineManager(
        const shaders::ShaderLibrary &library = shaders::builtin_shader_library())
      : _shader_library(library) {}
    ~MeshPipelineManager();

    MeshPipelineManager(const MeshPipelineManager &) = delete;
    auto operator=(const MeshPipelineManager &) -> MeshPipelineManager & = delete;

    // Must be called before anything else; again after release() for a new
    // device or render pass.
    void init(Film &film, uint32_t max_descriptor_sets = 32);
    auto shader_library() const -> const shaders::ShaderLibrary & { return _shader_library; }

    // ── Pipelines ───────────────────────────────────────────────────

    // A key with the Film's render-pass state filled in.
    auto make_key(const shaders::ShaderProgram &program,
                  shaders::ProgramVariant variant,
                  const std::array<vk::Format, max_vertex_streams> &stream_formats,
                  vk::PrimitiveTopology topology,
                  CullMode cull_mode,
                  bool depth_write) const -> MeshPipelineKey;
    // Created on first use; null if creation failed.
    auto pipeline(const MeshPipelineKey &key) -> vk::Pipeline;
    auto pipeline_layout(const shaders::ShaderProgram &program) -> vk::PipelineLayout;

    // ── Descriptor sets ─────────────────────────────────────────────
    //
    // Freeing requires that no command buffer in flight uses the set.

    auto allocate_scene_set() -> vk::DescriptorSet;
    void write_scene_set(vk::DescriptorSet set,
                         vk::Buffer transforms,
                         vk::DeviceSize transforms_size,
                         vk::Buffer lighting,
                         vk::DeviceSize lighting_size);
    void free_scene_set(vk::DescriptorSet set);

    auto allocate_material_set(const shaders::ShaderProgram &program) -> vk::DescriptorSet;
    // `material` holds the program's material blocks, read at a dynamic
    // offset; `textures` are its texture slots, in order.
    void write_material_set(const shaders::ShaderProgram &program,
                            vk::DescriptorSet set,
                            vk::Buffer material,
                            std::span<const vk::DescriptorImageInfo> textures);
    void free_material_set(const shaders::ShaderProgram &program, vk::DescriptorSet set);

    // GPU copies of material textures.
    auto texture_cache() -> TextureCache & { return _texture_cache; }

    // ── Lifecycle ───────────────────────────────────────────────────

    // Destroys the pipelines (e.g. when the render pass changes).
    void invalidate_pipelines();
    // Destroys everything.  Safe to call more than once.
    void release();
    bool is_initialized() const { return _initialized; }

  private:
    // Set 1's layout and the pipeline layout of one program.
    struct ProgramLayout {
        vk::DescriptorSetLayout material_set_layout;
        vk::PipelineLayout pipeline_layout;
        DescriptorAllocator allocator;
    };
    auto program_layout(const shaders::ShaderProgram &program) -> ProgramLayout &;
    auto create_pipeline(const MeshPipelineKey &key) -> vk::Pipeline;

    vk::Device _device;
    Film *_film = nullptr;
    shaders::ShaderLibrary _shader_library;
    std::uint64_t _shader_revision = 0;
    uint32_t _max_descriptor_sets = 32;

    vk::DescriptorSetLayout _scene_set_layout;
    DescriptorAllocator _scene_allocator;
    // By program id.
    std::unordered_map<std::string, ProgramLayout> _programs;
    TextureCache _texture_cache;
    std::unordered_map<MeshPipelineKey, vk::Pipeline, MeshPipelineKeyHash> _cache;
    bool _initialized = false;
};

} // namespace balsa::visualization::vulkan
