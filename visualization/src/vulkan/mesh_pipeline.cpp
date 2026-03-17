#include "balsa/visualization/vulkan/mesh_pipeline.hpp"
#include "balsa/visualization/vulkan/film.hpp"
#include "balsa/visualization/vulkan/mesh_buffers.hpp"
#include "balsa/visualization/shaders/mesh_shader.hpp"
#include "balsa/scene_graph/embedding_traits.hpp"
#include <spdlog/spdlog.h>
#include <stdexcept>

namespace balsa::visualization::vulkan {

// ── MeshPipelineKeyHash ──────────────────────────────────────────────

static std::size_t hash_combine(std::size_t seed, std::size_t v) {
    // boost::hash_combine style
    seed ^= v + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    return seed;
}

std::size_t MeshPipelineKeyHash::operator()(const MeshPipelineKey &k) const noexcept {
    std::size_t h = std::hash<uint8_t>{}(static_cast<uint8_t>(k.shading));
    h = hash_combine(h, std::hash<uint8_t>{}(static_cast<uint8_t>(k.color_source)));
    h = hash_combine(h, std::hash<uint8_t>{}(static_cast<uint8_t>(k.normal_source)));
    h = hash_combine(h, std::hash<uint8_t>{}(static_cast<uint8_t>(k.render_mode)));
    h = hash_combine(h, std::hash<std::string>{}(k.colormap_name));
    h = hash_combine(h, std::hash<bool>{}(k.has_normals));
    h = hash_combine(h, std::hash<bool>{}(k.has_colors));
    h = hash_combine(h, std::hash<bool>{}(k.has_scalars));
    h = hash_combine(h, std::hash<bool>{}(k.two_sided));
    h = hash_combine(h, std::hash<uint32_t>{}(k.msaa_samples));
    h = hash_combine(h, std::hash<uint64_t>{}(k.render_pass));
    h = hash_combine(h, std::hash<bool>{}(k.depth_test));
    return h;
}

// ── make_pipeline_key ────────────────────────────────────────────────

MeshPipelineKey make_pipeline_key(const MeshRenderState &state,
                                  const MeshBuffers &buffers,
                                  Film &film) {
    MeshPipelineKey key;
    key.shading = state.shading;
    key.color_source = state.color_source;
    key.normal_source = state.normal_source;
    key.render_mode = state.render_mode;

    // Normalise colormap name: only relevant for ScalarField
    key.colormap_name = (state.color_source == ColorSource::ScalarField)
                          ? state.colormap_name
                          : std::string{};

    key.has_normals = buffers.has_normals();
    key.has_colors = buffers.has_colors();
    key.has_scalars = buffers.has_scalars();

    key.two_sided = state.two_sided;

    key.msaa_samples = static_cast<uint32_t>(film.sample_count());
    key.render_pass = reinterpret_cast<uint64_t>(static_cast<VkRenderPass>(film.default_render_pass()));
    key.depth_test = film.has_depth_stencil();

    return key;
}

// ── MeshPipelineManager lifecycle ────────────────────────────────────

MeshPipelineManager::~MeshPipelineManager() {
    release();
}

MeshPipelineManager::MeshPipelineManager(MeshPipelineManager &&o) noexcept
  : _device(o._device),
    _film(o._film),
    _descriptor_set_layout(o._descriptor_set_layout),
    _pipeline_layout(o._pipeline_layout),
    _descriptor_pool(o._descriptor_pool),
    _cache(std::move(o._cache)),
    _initialized(o._initialized) {
    o._device = vk::Device{};
    o._film = nullptr;
    o._descriptor_set_layout = vk::DescriptorSetLayout{};
    o._pipeline_layout = vk::PipelineLayout{};
    o._descriptor_pool = vk::DescriptorPool{};
    o._initialized = false;
}

MeshPipelineManager &MeshPipelineManager::operator=(MeshPipelineManager &&o) noexcept {
    if (this != &o) {
        release();
        _device = o._device;
        _film = o._film;
        _descriptor_set_layout = o._descriptor_set_layout;
        _pipeline_layout = o._pipeline_layout;
        _descriptor_pool = o._descriptor_pool;
        _cache = std::move(o._cache);
        _initialized = o._initialized;
        o._device = vk::Device{};
        o._film = nullptr;
        o._descriptor_set_layout = vk::DescriptorSetLayout{};
        o._pipeline_layout = vk::PipelineLayout{};
        o._descriptor_pool = vk::DescriptorPool{};
        o._initialized = false;
    }
    return *this;
}

void MeshPipelineManager::init(Film &film, uint32_t max_descriptor_sets) {
    if (_initialized) {
        release();
    }
    _film = &film;
    _device = film.device();

    create_descriptor_set_layout();
    create_pipeline_layout();
    create_descriptor_pool(max_descriptor_sets);

    _initialized = true;
    spdlog::info("MeshPipelineManager: initialized");
}

void MeshPipelineManager::release() {
    if (!_initialized) {
        return;
    }
    if (_device) {
        _device.waitIdle();

        // Destroy cached pipelines
        for (auto &[key, pipe] : _cache) {
            if (pipe) {
                _device.destroyPipeline(pipe);
            }
        }
        _cache.clear();

        if (_descriptor_pool) {
            _device.destroyDescriptorPool(_descriptor_pool);
            _descriptor_pool = vk::DescriptorPool{};
        }
        if (_pipeline_layout) {
            _device.destroyPipelineLayout(_pipeline_layout);
            _pipeline_layout = vk::PipelineLayout{};
        }
        if (_descriptor_set_layout) {
            _device.destroyDescriptorSetLayout(_descriptor_set_layout);
            _descriptor_set_layout = vk::DescriptorSetLayout{};
        }
    }
    _device = vk::Device{};
    _film = nullptr;
    _initialized = false;
}

void MeshPipelineManager::invalidate_pipelines() {
    if (!_initialized) return;
    if (_device) {
        _device.waitIdle();
        for (auto &[key, pipe] : _cache) {
            if (pipe) {
                _device.destroyPipeline(pipe);
            }
        }
    }
    _cache.clear();
    spdlog::info("MeshPipelineManager: pipelines invalidated");
}

// ── Descriptor set layout / pipeline layout / descriptor pool ────────

void MeshPipelineManager::create_descriptor_set_layout() {
    // binding 0: TransformUBO (vertex shader only)
    // binding 1: MaterialUBO  (vertex + fragment shader)
    std::array<vk::DescriptorSetLayoutBinding, 2> bindings;

    auto &transform_binding = bindings[0];
    transform_binding.setBinding(0);
    transform_binding.setDescriptorType(vk::DescriptorType::eUniformBuffer);
    transform_binding.setDescriptorCount(1);
    transform_binding.setStageFlags(vk::ShaderStageFlagBits::eVertex);

    auto &material_binding = bindings[1];
    material_binding.setBinding(1);
    material_binding.setDescriptorType(vk::DescriptorType::eUniformBuffer);
    material_binding.setDescriptorCount(1);
    material_binding.setStageFlags(vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment);

    vk::DescriptorSetLayoutCreateInfo ci;
    ci.setBindings(bindings);
    _descriptor_set_layout = _device.createDescriptorSetLayout(ci);
}

void MeshPipelineManager::create_pipeline_layout() {
    vk::PipelineLayoutCreateInfo ci;
    ci.setSetLayouts(_descriptor_set_layout);
    ci.setPushConstantRangeCount(0);
    _pipeline_layout = _device.createPipelineLayout(ci);
}

void MeshPipelineManager::create_descriptor_pool(uint32_t max_sets) {
    // Each set has 2 uniform buffer descriptors
    vk::DescriptorPoolSize pool_size;
    pool_size.setType(vk::DescriptorType::eUniformBuffer);
    pool_size.setDescriptorCount(max_sets * 2);

    vk::DescriptorPoolCreateInfo ci;
    ci.setFlags(vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet);
    ci.setMaxSets(max_sets);
    ci.setPoolSizes(pool_size);
    _descriptor_pool = _device.createDescriptorPool(ci);
}

// ── Descriptor set allocation / writing ──────────────────────────────

vk::DescriptorSet MeshPipelineManager::allocate_descriptor_set() {
    if (!_initialized) {
        throw std::runtime_error("MeshPipelineManager: not initialized");
    }
    vk::DescriptorSetAllocateInfo ai;
    ai.setDescriptorPool(_descriptor_pool);
    ai.setSetLayouts(_descriptor_set_layout);
    auto sets = _device.allocateDescriptorSets(ai);
    return sets[0];
}

void MeshPipelineManager::write_descriptor_set(vk::DescriptorSet ds,
                                               vk::Buffer transform_buffer,
                                               vk::DeviceSize transform_size,
                                               vk::Buffer material_buffer,
                                               vk::DeviceSize material_size) {
    vk::DescriptorBufferInfo transform_info;
    transform_info.setBuffer(transform_buffer);
    transform_info.setOffset(0);
    transform_info.setRange(transform_size);

    vk::DescriptorBufferInfo material_info;
    material_info.setBuffer(material_buffer);
    material_info.setOffset(0);
    material_info.setRange(material_size);

    std::array<vk::WriteDescriptorSet, 2> writes;

    auto &w0 = writes[0];
    w0.setDstSet(ds);
    w0.setDstBinding(0);
    w0.setDstArrayElement(0);
    w0.setDescriptorType(vk::DescriptorType::eUniformBuffer);
    w0.setDescriptorCount(1);
    w0.setPBufferInfo(&transform_info);

    auto &w1 = writes[1];
    w1.setDstSet(ds);
    w1.setDstBinding(1);
    w1.setDstArrayElement(0);
    w1.setDescriptorType(vk::DescriptorType::eUniformBuffer);
    w1.setDescriptorCount(1);
    w1.setPBufferInfo(&material_info);

    _device.updateDescriptorSets(writes, {});
}

// ── Pipeline access ──────────────────────────────────────────────────

vk::Pipeline MeshPipelineManager::get_or_create(const MeshRenderState &state,
                                                const MeshBuffers &buffers,
                                                Film &film) {
    if (!_initialized) {
        throw std::runtime_error("MeshPipelineManager: not initialized");
    }
    auto key = make_pipeline_key(state, buffers, film);
    auto it = _cache.find(key);
    if (it != _cache.end()) {
        return it->second;
    }
    auto pipeline = create_pipeline(key);
    _cache[key] = pipeline;
    return pipeline;
}

// ── Pipeline creation ────────────────────────────────────────────────

vk::Pipeline MeshPipelineManager::create_pipeline(const MeshPipelineKey &key) {
    spdlog::info("MeshPipelineManager: creating pipeline for shading={}, color={}, render_mode={}",
                 static_cast<int>(key.shading),
                 static_cast<int>(key.color_source),
                 static_cast<int>(key.render_mode));

    // Build a temporary MeshRenderState from the key to drive shader compilation
    MeshRenderState temp_state;
    temp_state.shading = key.shading;
    temp_state.color_source = key.color_source;
    temp_state.normal_source = key.normal_source;
    temp_state.render_mode = key.render_mode;
    temp_state.colormap_name = key.colormap_name;

    using ET3F = scene_graph::embedding_traits3F;
    shaders::MeshShader<ET3F> shader(temp_state);
    auto vert_spv = shader.vert_spirv();
    auto frag_spv = shader.frag_spirv();

    // Create shader modules
    auto create_shader_module = [&](const std::vector<uint32_t> &spv) -> vk::ShaderModule {
        vk::ShaderModuleCreateInfo ci;
        ci.setCodeSize(sizeof(uint32_t) * spv.size());
        ci.setPCode(spv.data());
        return _device.createShaderModule(ci);
    };

    vk::ShaderModule vert_module = create_shader_module(vert_spv);
    vk::ShaderModule frag_module = create_shader_module(frag_spv);

    // Shader stages
    const std::string entry = "main";
    vk::PipelineShaderStageCreateInfo shader_stages[2];
    {
        auto &vs = shader_stages[0];
        vs.setStage(vk::ShaderStageFlagBits::eVertex);
        vs.setModule(vert_module);
        vs.setPName(entry.c_str());

        auto &fs = shader_stages[1];
        fs.setStage(vk::ShaderStageFlagBits::eFragment);
        fs.setModule(frag_module);
        fs.setPName(entry.c_str());
    }

    // ── Vertex input ────────────────────────────────────────────────
    //
    // Build binding/attribute descriptions from the key's attribute flags,
    // matching the layout in mesh.vert:
    //   binding 0: vec3 position  (location 0)
    //   binding 1: vec3 normal    (location 1)  — if has_normals
    //   binding 2: vec3 color     (location 2)  — if has_colors
    //   binding 3: float scalar   (location 3)  — if has_scalars

    std::vector<vk::VertexInputBindingDescription> binding_descs;
    std::vector<vk::VertexInputAttributeDescription> attrib_descs;

    // Positions always present
    binding_descs.push_back({ 0, sizeof(float) * 3, vk::VertexInputRate::eVertex });
    attrib_descs.push_back({ 0, 0, vk::Format::eR32G32B32Sfloat, 0 });

    if (key.has_normals) {
        binding_descs.push_back({ 1, sizeof(float) * 3, vk::VertexInputRate::eVertex });
        attrib_descs.push_back({ 1, 1, vk::Format::eR32G32B32Sfloat, 0 });
    }
    if (key.has_colors) {
        binding_descs.push_back({ 2, sizeof(float) * 3, vk::VertexInputRate::eVertex });
        attrib_descs.push_back({ 2, 2, vk::Format::eR32G32B32Sfloat, 0 });
    }
    if (key.has_scalars) {
        binding_descs.push_back({ 3, sizeof(float), vk::VertexInputRate::eVertex });
        attrib_descs.push_back({ 3, 3, vk::Format::eR32Sfloat, 0 });
    }

    vk::PipelineVertexInputStateCreateInfo vertex_input;
    vertex_input.setVertexBindingDescriptions(binding_descs);
    vertex_input.setVertexAttributeDescriptions(attrib_descs);

    // ── Input assembly ──────────────────────────────────────────────

    vk::PipelineInputAssemblyStateCreateInfo input_assembly;
    switch (key.render_mode) {
    case RenderMode::Solid:
    case RenderMode::SolidWireframe:
        input_assembly.setTopology(vk::PrimitiveTopology::eTriangleList);
        break;
    case RenderMode::Wireframe:
        input_assembly.setTopology(vk::PrimitiveTopology::eLineList);
        break;
    case RenderMode::Points:
        input_assembly.setTopology(vk::PrimitiveTopology::ePointList);
        break;
    }
    input_assembly.setPrimitiveRestartEnable(VK_FALSE);

    // ── Viewport / scissor (dynamic) ────────────────────────────────

    vk::PipelineViewportStateCreateInfo viewport_state;
    viewport_state.setViewportCount(1);
    viewport_state.setScissorCount(1);

    // ── Rasterization ───────────────────────────────────────────────

    vk::PipelineRasterizationStateCreateInfo rasterization;
    rasterization.setDepthClampEnable(VK_FALSE);
    rasterization.setRasterizerDiscardEnable(VK_FALSE);
    rasterization.setPolygonMode(vk::PolygonMode::eFill);
    rasterization.setCullMode(key.two_sided ? vk::CullModeFlagBits::eNone : vk::CullModeFlagBits::eBack);
    rasterization.setFrontFace(vk::FrontFace::eCounterClockwise);
    rasterization.setDepthBiasEnable(VK_FALSE);
    rasterization.setLineWidth(1.0f);

    // ── Multisample ─────────────────────────────────────────────────

    vk::PipelineMultisampleStateCreateInfo multisampling;
    multisampling.setRasterizationSamples(
      static_cast<vk::SampleCountFlagBits>(key.msaa_samples));
    multisampling.setSampleShadingEnable(VK_FALSE);
    multisampling.setMinSampleShading(1.0f);

    // ── Depth / stencil ─────────────────────────────────────────────

    vk::PipelineDepthStencilStateCreateInfo depth_stencil;
    depth_stencil.setDepthTestEnable(key.depth_test ? VK_TRUE : VK_FALSE);
    depth_stencil.setDepthWriteEnable(key.depth_test ? VK_TRUE : VK_FALSE);
    depth_stencil.setDepthCompareOp(vk::CompareOp::eLessOrEqual);
    depth_stencil.setDepthBoundsTestEnable(VK_FALSE);
    depth_stencil.setStencilTestEnable(VK_FALSE);

    // ── Color blending ──────────────────────────────────────────────

    vk::PipelineColorBlendAttachmentState color_blend_attachment;
    color_blend_attachment.setColorWriteMask(
      vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA);
    color_blend_attachment.setBlendEnable(VK_TRUE);
    color_blend_attachment.setSrcColorBlendFactor(vk::BlendFactor::eSrcAlpha);
    color_blend_attachment.setDstColorBlendFactor(vk::BlendFactor::eOneMinusSrcAlpha);
    color_blend_attachment.setColorBlendOp(vk::BlendOp::eAdd);
    color_blend_attachment.setSrcAlphaBlendFactor(vk::BlendFactor::eOne);
    color_blend_attachment.setDstAlphaBlendFactor(vk::BlendFactor::eZero);
    color_blend_attachment.setAlphaBlendOp(vk::BlendOp::eAdd);

    vk::PipelineColorBlendStateCreateInfo color_blend;
    color_blend.setLogicOpEnable(VK_FALSE);
    color_blend.setAttachmentCount(1);
    color_blend.setPAttachments(&color_blend_attachment);

    // ── Dynamic state ───────────────────────────────────────────────

    std::vector<vk::DynamicState> dynamic_states = {
        vk::DynamicState::eViewport,
        vk::DynamicState::eScissor,
        vk::DynamicState::eLineWidth,
    };
    vk::PipelineDynamicStateCreateInfo dynamic_state;
    dynamic_state.setDynamicStates(dynamic_states);

    // ── Assemble ────────────────────────────────────────────────────

    vk::RenderPass rp{ reinterpret_cast<VkRenderPass>(key.render_pass) };

    vk::GraphicsPipelineCreateInfo pipeline_info;
    pipeline_info.setStageCount(2);
    pipeline_info.setPStages(shader_stages);
    pipeline_info.setPVertexInputState(&vertex_input);
    pipeline_info.setPInputAssemblyState(&input_assembly);
    pipeline_info.setPViewportState(&viewport_state);
    pipeline_info.setPRasterizationState(&rasterization);
    pipeline_info.setPMultisampleState(&multisampling);
    pipeline_info.setPDepthStencilState(&depth_stencil);
    pipeline_info.setPColorBlendState(&color_blend);
    pipeline_info.setPDynamicState(&dynamic_state);
    pipeline_info.setLayout(_pipeline_layout);
    pipeline_info.setRenderPass(rp);
    pipeline_info.setSubpass(0);
    pipeline_info.setBasePipelineHandle(VK_NULL_HANDLE);

    vk::Pipeline result;
    auto res = _device.createGraphicsPipeline(VK_NULL_HANDLE, pipeline_info);
    if (res.result == vk::Result::eSuccess) {
        result = res.value;
        spdlog::info("MeshPipelineManager: pipeline created successfully");
    } else {
        spdlog::error("MeshPipelineManager: failed to create graphics pipeline");
    }

    // Cleanup shader modules
    _device.destroyShaderModule(vert_module);
    _device.destroyShaderModule(frag_module);

    return result;
}

}// namespace balsa::visualization::vulkan
