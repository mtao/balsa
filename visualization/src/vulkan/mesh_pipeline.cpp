#include "balsa/visualization/vulkan/mesh_pipeline.hpp"
#include "balsa/visualization/vulkan/film.hpp"
#include "balsa/visualization/vulkan/vertex_format.hpp"

#include <spdlog/spdlog.h>

#include <functional>
#include <stdexcept>
#include <vector>

namespace balsa::visualization::vulkan {

namespace {
    auto hash_combine(std::size_t seed, std::size_t value) -> std::size_t {
        return seed ^ (value + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2));
    }

    auto program_definition(std::string_view id) -> const shaders::ShaderProgramDefinition & {
        const auto program = shaders::shader_programs().find(id);
        if (!program) throw std::runtime_error("MeshPipelineManager: unknown shader program");
        return program->value();
    }
} // namespace

auto MeshPipelineKeyHash::operator()(const MeshPipelineKey &key) const noexcept -> std::size_t {
    const std::hash<std::string> string_hash;
    std::size_t h = string_hash(key.program);
    for (const auto &define : key.variant.defines) h = hash_combine(h, string_hash(define));
    for (const auto &[name, target] : key.variant.includes) {
        h = hash_combine(h, string_hash(name));
        h = hash_combine(h, string_hash(target));
    }
    for (const auto format : key.stream_formats) h = hash_combine(h, std::hash<vk::Format>{}(format));
    h = hash_combine(h, static_cast<std::size_t>(key.topology));
    h = hash_combine(h, static_cast<std::size_t>(key.cull_mode));
    h = hash_combine(h, key.depth_write);
    h = hash_combine(h, key.msaa_samples);
    h = hash_combine(h, std::hash<uint64_t>{}(key.render_pass));
    return hash_combine(h, key.depth_test);
}

MeshPipelineManager::~MeshPipelineManager() { release(); }

void MeshPipelineManager::init(Film &film, uint32_t max_descriptor_sets) {
    if (_initialized) release();
    _film = &film;
    _device = film.device();
    _max_descriptor_sets = max_descriptor_sets;
    _shader_revision = _shader_library.revision();

    // Set 0: transforms (binding 0) and scene lighting (binding 1).
    const auto stages = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment;
    const std::array<vk::DescriptorSetLayoutBinding, 2> scene_bindings{{
        {0, vk::DescriptorType::eUniformBuffer, 1, stages},
        {1, vk::DescriptorType::eUniformBuffer, 1, stages},
    }};
    _scene_set_layout = _device.createDescriptorSetLayout(
        vk::DescriptorSetLayoutCreateInfo{}.setBindings(scene_bindings));
    _scene_allocator.init(_device, {{vk::DescriptorType::eUniformBuffer, 2}}, max_descriptor_sets);

    _initialized = true;
    spdlog::info("MeshPipelineManager: initialized");
}

void MeshPipelineManager::release() {
    if (!_initialized) return;
    if (_device) {
        _device.waitIdle();
        for (auto &[key, pipeline] : _cache) {
            if (pipeline) _device.destroyPipeline(pipeline);
        }
        _cache.clear();
        _texture_cache.release();
        for (auto &[id, layout] : _programs) {
            layout.allocator.release();
            _device.destroyPipelineLayout(layout.pipeline_layout);
            _device.destroyDescriptorSetLayout(layout.material_set_layout);
        }
        _programs.clear();
        _scene_allocator.release();
        _device.destroyDescriptorSetLayout(_scene_set_layout);
        _scene_set_layout = vk::DescriptorSetLayout{};
    }
    _device = vk::Device{};
    _film = nullptr;
    _initialized = false;
}

void MeshPipelineManager::invalidate_pipelines() {
    if (!_initialized) return;
    if (_device) {
        _device.waitIdle();
        for (auto &[key, pipeline] : _cache) {
            if (pipeline) _device.destroyPipeline(pipeline);
        }
    }
    _cache.clear();
    spdlog::info("MeshPipelineManager: pipelines invalidated");
}

// ── Layouts and descriptor sets ──────────────────────────────────────

auto MeshPipelineManager::program_layout(const shaders::ShaderProgram &program)
    -> ProgramLayout & {
    if (!_initialized) throw std::runtime_error("MeshPipelineManager: not initialized");
    const auto [it, inserted] = _programs.try_emplace(std::string(program.id()));
    auto &layout = it->second;
    if (!inserted) return layout;

    // Set 1: the material block (binding 0), then the textures.
    const auto &definition = program.value();
    std::vector<vk::DescriptorSetLayoutBinding> bindings{
        {0, vk::DescriptorType::eUniformBufferDynamic, 1,
         vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment}};
    for (uint32_t i = 0; i < definition.textures.size(); ++i) {
        bindings.push_back({i + 1, vk::DescriptorType::eCombinedImageSampler, 1,
                            vk::ShaderStageFlagBits::eFragment});
    }
    layout.material_set_layout =
        _device.createDescriptorSetLayout(vk::DescriptorSetLayoutCreateInfo{}.setBindings(bindings));
    const std::array set_layouts{_scene_set_layout, layout.material_set_layout};
    layout.pipeline_layout =
        _device.createPipelineLayout(vk::PipelineLayoutCreateInfo{}.setSetLayouts(set_layouts));

    std::vector<DescriptorAllocator::Descriptors> per_set{
        {vk::DescriptorType::eUniformBufferDynamic, 1}};
    if (!definition.textures.empty()) {
        per_set.push_back({vk::DescriptorType::eCombinedImageSampler,
                           static_cast<uint32_t>(definition.textures.size())});
    }
    layout.allocator.init(_device, std::move(per_set), _max_descriptor_sets);
    return layout;
}

auto MeshPipelineManager::pipeline_layout(const shaders::ShaderProgram &program)
    -> vk::PipelineLayout {
    return program_layout(program).pipeline_layout;
}

auto MeshPipelineManager::allocate_scene_set() -> vk::DescriptorSet {
    if (!_initialized) throw std::runtime_error("MeshPipelineManager: not initialized");
    return _scene_allocator.allocate(_scene_set_layout);
}

void MeshPipelineManager::write_scene_set(vk::DescriptorSet set,
                                          vk::Buffer transforms,
                                          vk::DeviceSize transforms_size,
                                          vk::Buffer lighting,
                                          vk::DeviceSize lighting_size) {
    const vk::DescriptorBufferInfo transforms_info{transforms, 0, transforms_size};
    const vk::DescriptorBufferInfo lighting_info{lighting, 0, lighting_size};
    const std::array writes{
        vk::WriteDescriptorSet{set, 0, 0, 1, vk::DescriptorType::eUniformBuffer, nullptr,
                               &transforms_info},
        vk::WriteDescriptorSet{set, 1, 0, 1, vk::DescriptorType::eUniformBuffer, nullptr,
                               &lighting_info},
    };
    _device.updateDescriptorSets(writes, {});
}

void MeshPipelineManager::free_scene_set(vk::DescriptorSet set) {
    if (_initialized && set) _scene_allocator.free(set);
}

auto MeshPipelineManager::allocate_material_set(const shaders::ShaderProgram &program)
    -> vk::DescriptorSet {
    auto &layout = program_layout(program);
    return layout.allocator.allocate(layout.material_set_layout);
}

void MeshPipelineManager::write_material_set(const shaders::ShaderProgram &program,
                                             vk::DescriptorSet set,
                                             vk::Buffer material,
                                             std::span<const vk::DescriptorImageInfo> textures) {
    // The dynamic offset selects the block; the range is one block.
    const vk::DescriptorBufferInfo material_info{material, 0, program.value().material_size};
    std::vector<vk::WriteDescriptorSet> writes{vk::WriteDescriptorSet{
        set, 0, 0, 1, vk::DescriptorType::eUniformBufferDynamic, nullptr, &material_info}};
    for (uint32_t i = 0; i < textures.size(); ++i) {
        writes.push_back({set, i + 1, 0, 1, vk::DescriptorType::eCombinedImageSampler,
                          &textures[i]});
    }
    _device.updateDescriptorSets(writes, {});
}

void MeshPipelineManager::free_material_set(const shaders::ShaderProgram &program,
                                            vk::DescriptorSet set) {
    if (!_initialized || !set) return;
    if (const auto it = _programs.find(std::string(program.id())); it != _programs.end()) {
        it->second.allocator.free(set);
    }
}

// ── Pipelines ────────────────────────────────────────────────────────

auto MeshPipelineManager::make_key(const shaders::ShaderProgram &program,
                                   shaders::ProgramVariant variant,
                                   const std::array<vk::Format, max_vertex_streams> &stream_formats,
                                   vk::PrimitiveTopology topology,
                                   CullMode cull_mode,
                                   bool depth_write) const -> MeshPipelineKey {
    return {.program = std::string(program.id()),
            .variant = std::move(variant),
            .stream_formats = stream_formats,
            .topology = topology,
            .cull_mode = cull_mode,
            .depth_write = depth_write,
            .msaa_samples = static_cast<uint32_t>(_film->sample_count()),
            .render_pass = reinterpret_cast<uint64_t>(
                static_cast<VkRenderPass>(_film->default_render_pass())),
            .depth_test = _film->has_depth_stencil()};
}

auto MeshPipelineManager::pipeline(const MeshPipelineKey &key) -> vk::Pipeline {
    if (!_initialized) throw std::runtime_error("MeshPipelineManager: not initialized");
    if (_shader_revision != _shader_library.revision()) {
        invalidate_pipelines();
        _shader_revision = _shader_library.revision();
    }
    if (const auto it = _cache.find(key); it != _cache.end()) return it->second;
    const auto created = create_pipeline(key);
    if (created) _cache.emplace(key, created);
    return created;
}

auto MeshPipelineManager::create_pipeline(const MeshPipelineKey &key) -> vk::Pipeline {
    const auto &definition = program_definition(key.program);
    spdlog::info("MeshPipelineManager: creating a pipeline for {} (topology {})", definition.id,
                 static_cast<int>(key.topology));

    const shaders::ProgramShader shader(definition, key.variant, _shader_library);
    const auto vert_spv = shader.vert_spirv();
    const auto frag_spv = shader.frag_spirv();
    if (vert_spv.empty() || frag_spv.empty()) {
        spdlog::error("MeshPipelineManager: shader compilation failed; no pipeline");
        return vk::Pipeline{};
    }
    const auto module = [&](const std::vector<uint32_t> &spv) {
        return _device.createShaderModule(vk::ShaderModuleCreateInfo{}.setCode(spv));
    };
    const vk::ShaderModule vert_module = module(vert_spv);
    const vk::ShaderModule frag_module = module(frag_spv);
    const std::array shader_stages{
        vk::PipelineShaderStageCreateInfo{{}, vk::ShaderStageFlagBits::eVertex, vert_module, "main"},
        vk::PipelineShaderStageCreateInfo{{}, vk::ShaderStageFlagBits::eFragment, frag_module, "main"},
    };

    // One binding per bound stream, at its location.
    std::vector<vk::VertexInputBindingDescription> bindings;
    std::vector<vk::VertexInputAttributeDescription> attributes;
    for (uint32_t location = 0; location < key.stream_formats.size(); ++location) {
        const auto format = key.stream_formats[location];
        if (format == vk::Format::eUndefined) continue;
        bindings.push_back({location, vertex_format_size(format), vk::VertexInputRate::eVertex});
        attributes.push_back({location, location, format, 0});
    }
    const auto vertex_input = vk::PipelineVertexInputStateCreateInfo{}
                                  .setVertexBindingDescriptions(bindings)
                                  .setVertexAttributeDescriptions(attributes);
    const auto input_assembly = vk::PipelineInputAssemblyStateCreateInfo{}.setTopology(key.topology);
    const auto viewport_state =
        vk::PipelineViewportStateCreateInfo{}.setViewportCount(1).setScissorCount(1);

    vk::CullModeFlags cull_mode = vk::CullModeFlagBits::eNone;
    if (key.cull_mode == CullMode::Back) cull_mode = vk::CullModeFlagBits::eBack;
    if (key.cull_mode == CullMode::Front) cull_mode = vk::CullModeFlagBits::eFront;
    const auto rasterization =
        vk::PipelineRasterizationStateCreateInfo{}
            .setPolygonMode(vk::PolygonMode::eFill)
            .setCullMode(cull_mode)
            .setFrontFace(vk::FrontFace::eCounterClockwise)
            .setDepthBiasEnable(key.topology == vk::PrimitiveTopology::eTriangleList)
            .setLineWidth(1.0f);
    const auto multisampling = vk::PipelineMultisampleStateCreateInfo{}.setRasterizationSamples(
        static_cast<vk::SampleCountFlagBits>(key.msaa_samples));
    const auto depth_stencil = vk::PipelineDepthStencilStateCreateInfo{}
                                   .setDepthTestEnable(key.depth_test)
                                   .setDepthWriteEnable(key.depth_test && key.depth_write)
                                   .setDepthCompareOp(vk::CompareOp::eLessOrEqual);

    // Alpha blending; opaque draws output alpha 1.
    const auto blend_attachment =
        vk::PipelineColorBlendAttachmentState{}
            .setBlendEnable(true)
            .setSrcColorBlendFactor(vk::BlendFactor::eSrcAlpha)
            .setDstColorBlendFactor(vk::BlendFactor::eOneMinusSrcAlpha)
            .setColorBlendOp(vk::BlendOp::eAdd)
            .setSrcAlphaBlendFactor(vk::BlendFactor::eOne)
            .setDstAlphaBlendFactor(vk::BlendFactor::eZero)
            .setAlphaBlendOp(vk::BlendOp::eAdd)
            .setColorWriteMask(vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG
                               | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA);
    const auto color_blend = vk::PipelineColorBlendStateCreateInfo{}.setAttachments(blend_attachment);

    const std::array dynamic_states{vk::DynamicState::eViewport, vk::DynamicState::eScissor,
                                    vk::DynamicState::eLineWidth, vk::DynamicState::eDepthBias};
    const auto dynamic_state = vk::PipelineDynamicStateCreateInfo{}.setDynamicStates(dynamic_states);

    const auto info = vk::GraphicsPipelineCreateInfo{}
                          .setStages(shader_stages)
                          .setPVertexInputState(&vertex_input)
                          .setPInputAssemblyState(&input_assembly)
                          .setPViewportState(&viewport_state)
                          .setPRasterizationState(&rasterization)
                          .setPMultisampleState(&multisampling)
                          .setPDepthStencilState(&depth_stencil)
                          .setPColorBlendState(&color_blend)
                          .setPDynamicState(&dynamic_state)
                          .setLayout(pipeline_layout(*shaders::shader_programs().find(key.program)))
                          .setRenderPass(vk::RenderPass{reinterpret_cast<VkRenderPass>(key.render_pass)})
                          .setSubpass(0);

    vk::Pipeline result;
    if (const auto created = _device.createGraphicsPipeline(nullptr, info);
        created.result == vk::Result::eSuccess) {
        result = created.value;
    } else {
        spdlog::error("MeshPipelineManager: failed to create a graphics pipeline");
    }
    _device.destroyShaderModule(vert_module);
    _device.destroyShaderModule(frag_module);
    return result;
}

} // namespace balsa::visualization::vulkan
