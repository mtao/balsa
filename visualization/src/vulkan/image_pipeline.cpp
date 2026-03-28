#include "balsa/visualization/vulkan/image_pipeline.hpp"
#include "balsa/visualization/shaders/abstract_shader.hpp"
#include "balsa/visualization/vulkan/film.hpp"

#include <spdlog/spdlog.h>
#include <stdexcept>

namespace balsa::visualization::vulkan {

// ── ImagePipelineManager lifecycle ──────────────────────────────────

ImagePipelineManager::~ImagePipelineManager() { release(); }

ImagePipelineManager::ImagePipelineManager(ImagePipelineManager &&o) noexcept
  : _device(o._device), _film(o._film),
    _descriptor_set_layout(o._descriptor_set_layout),
    _pipeline_layout(o._pipeline_layout), _descriptor_pool(o._descriptor_pool),
    _pipeline(o._pipeline), _cached_render_pass(o._cached_render_pass),
    _cached_msaa_samples(o._cached_msaa_samples),
    _cached_depth_test(o._cached_depth_test), _initialized(o._initialized) {
    o._device = vk::Device{};
    o._film = nullptr;
    o._descriptor_set_layout = vk::DescriptorSetLayout{};
    o._pipeline_layout = vk::PipelineLayout{};
    o._descriptor_pool = vk::DescriptorPool{};
    o._pipeline = vk::Pipeline{};
    o._initialized = false;
}

ImagePipelineManager &
    ImagePipelineManager::operator=(ImagePipelineManager &&o) noexcept {
    if (this != &o) {
        release();
        _device = o._device;
        _film = o._film;
        _descriptor_set_layout = o._descriptor_set_layout;
        _pipeline_layout = o._pipeline_layout;
        _descriptor_pool = o._descriptor_pool;
        _pipeline = o._pipeline;
        _cached_render_pass = o._cached_render_pass;
        _cached_msaa_samples = o._cached_msaa_samples;
        _cached_depth_test = o._cached_depth_test;
        _initialized = o._initialized;
        o._device = vk::Device{};
        o._film = nullptr;
        o._descriptor_set_layout = vk::DescriptorSetLayout{};
        o._pipeline_layout = vk::PipelineLayout{};
        o._descriptor_pool = vk::DescriptorPool{};
        o._pipeline = vk::Pipeline{};
        o._initialized = false;
    }
    return *this;
}

void ImagePipelineManager::init(Film &film, uint32_t max_descriptor_sets) {
    if (_initialized) { release(); }
    _film = &film;
    _device = film.device();

    create_descriptor_set_layout();
    create_pipeline_layout();
    create_descriptor_pool(max_descriptor_sets);

    _initialized = true;
    spdlog::info("ImagePipelineManager: initialized");
}

void ImagePipelineManager::release() {
    if (!_initialized) { return; }
    if (_device) {
        _device.waitIdle();

        if (_pipeline) {
            _device.destroyPipeline(_pipeline);
            _pipeline = vk::Pipeline{};
        }

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

void ImagePipelineManager::invalidate_pipeline() {
    if (!_initialized) return;
    if (_device && _pipeline) {
        _device.waitIdle();
        _device.destroyPipeline(_pipeline);
        _pipeline = vk::Pipeline{};
    }
    _cached_render_pass = 0;
    _cached_msaa_samples = 0;
    _cached_depth_test = false;
    spdlog::info("ImagePipelineManager: pipeline invalidated");
}

// ── Descriptor set layout / pipeline layout / pool ──────────────────

void ImagePipelineManager::create_descriptor_set_layout() {
    // binding 0: ImageTransformUBO (vertex stage)
    // binding 1: ImageParamsUBO    (fragment stage)
    // binding 2: combined image sampler (fragment stage)
    std::array<vk::DescriptorSetLayoutBinding, 3> bindings;

    auto &transform_binding = bindings[0];
    transform_binding.setBinding(0);
    transform_binding.setDescriptorType(vk::DescriptorType::eUniformBuffer);
    transform_binding.setDescriptorCount(1);
    transform_binding.setStageFlags(vk::ShaderStageFlagBits::eVertex);

    auto &params_binding = bindings[1];
    params_binding.setBinding(1);
    params_binding.setDescriptorType(vk::DescriptorType::eUniformBuffer);
    params_binding.setDescriptorCount(1);
    params_binding.setStageFlags(vk::ShaderStageFlagBits::eFragment);

    auto &sampler_binding = bindings[2];
    sampler_binding.setBinding(2);
    sampler_binding.setDescriptorType(
        vk::DescriptorType::eCombinedImageSampler);
    sampler_binding.setDescriptorCount(1);
    sampler_binding.setStageFlags(vk::ShaderStageFlagBits::eFragment);

    vk::DescriptorSetLayoutCreateInfo ci;
    ci.setBindings(bindings);
    _descriptor_set_layout = _device.createDescriptorSetLayout(ci);
}

void ImagePipelineManager::create_pipeline_layout() {
    vk::PipelineLayoutCreateInfo ci;
    ci.setSetLayouts(_descriptor_set_layout);
    ci.setPushConstantRangeCount(0);
    _pipeline_layout = _device.createPipelineLayout(ci);
}

void ImagePipelineManager::create_descriptor_pool(uint32_t max_sets) {
    // Each set has 2 uniform buffers + 1 combined image sampler.
    std::array<vk::DescriptorPoolSize, 2> pool_sizes;
    pool_sizes[0].setType(vk::DescriptorType::eUniformBuffer);
    pool_sizes[0].setDescriptorCount(max_sets * 2); // 2 UBOs per set
    pool_sizes[1].setType(vk::DescriptorType::eCombinedImageSampler);
    pool_sizes[1].setDescriptorCount(max_sets); // 1 sampler per set

    vk::DescriptorPoolCreateInfo ci;
    ci.setFlags(vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet);
    ci.setMaxSets(max_sets);
    ci.setPoolSizes(pool_sizes);
    _descriptor_pool = _device.createDescriptorPool(ci);
}

// ── Descriptor set allocation / writing / freeing ────────────────────

vk::DescriptorSet ImagePipelineManager::allocate_descriptor_set() {
    if (!_initialized) {
        throw std::runtime_error("ImagePipelineManager: not initialized");
    }
    vk::DescriptorSetAllocateInfo ai;
    ai.setDescriptorPool(_descriptor_pool);
    ai.setSetLayouts(_descriptor_set_layout);
    auto sets = _device.allocateDescriptorSets(ai);
    return sets[0];
}

void ImagePipelineManager::write_descriptor_set(vk::DescriptorSet ds,
                                                vk::Buffer transform_buffer,
                                                vk::DeviceSize transform_size,
                                                vk::Buffer params_buffer,
                                                vk::DeviceSize params_size,
                                                vk::ImageView image_view,
                                                vk::Sampler sampler) {
    vk::DescriptorBufferInfo transform_info;
    transform_info.setBuffer(transform_buffer);
    transform_info.setOffset(0);
    transform_info.setRange(transform_size);

    vk::DescriptorBufferInfo params_info;
    params_info.setBuffer(params_buffer);
    params_info.setOffset(0);
    params_info.setRange(params_size);

    vk::DescriptorImageInfo image_info;
    image_info.setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal);
    image_info.setImageView(image_view);
    image_info.setSampler(sampler);

    std::array<vk::WriteDescriptorSet, 3> writes;

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
    w1.setPBufferInfo(&params_info);

    auto &w2 = writes[2];
    w2.setDstSet(ds);
    w2.setDstBinding(2);
    w2.setDstArrayElement(0);
    w2.setDescriptorType(vk::DescriptorType::eCombinedImageSampler);
    w2.setDescriptorCount(1);
    w2.setPImageInfo(&image_info);

    _device.updateDescriptorSets(writes, {});
}

void ImagePipelineManager::free_descriptor_set(vk::DescriptorSet ds) {
    if (!_initialized || !ds) return;
    _device.freeDescriptorSets(_descriptor_pool, {ds});
}

// ── Pipeline access ─────────────────────────────────────────────────

vk::Pipeline ImagePipelineManager::get_or_create(Film &film) {
    if (!_initialized) {
        throw std::runtime_error("ImagePipelineManager: not initialized");
    }

    // Check if the cached pipeline is still valid for this render pass.
    uint64_t rp = reinterpret_cast<uint64_t>(
        static_cast<VkRenderPass>(film.default_render_pass()));
    uint32_t msaa = static_cast<uint32_t>(film.sample_count());
    bool depth = film.has_depth_stencil();

    if (_pipeline && rp == _cached_render_pass && msaa == _cached_msaa_samples
        && depth == _cached_depth_test) {
        return _pipeline;
    }

    // Invalidate and recreate.
    if (_pipeline) {
        _device.destroyPipeline(_pipeline);
        _pipeline = vk::Pipeline{};
    }

    _cached_render_pass = rp;
    _cached_msaa_samples = msaa;
    _cached_depth_test = depth;

    _pipeline = create_pipeline();
    return _pipeline;
}

// ── Pipeline creation ───────────────────────────────────────────────

vk::Pipeline ImagePipelineManager::create_pipeline() {
    spdlog::info("ImagePipelineManager: creating pipeline");

    // Compile shaders from Qt resources.
    shaders::AbstractShader shader_compiler;
    auto vert_spv = shader_compiler.compile_glsl_from_path(
        ":/glsl/image.vert", shaders::AbstractShader::ShaderType::Vertex);
    auto frag_spv = shader_compiler.compile_glsl_from_path(
        ":/glsl/image.frag", shaders::AbstractShader::ShaderType::Fragment);

    if (vert_spv.empty() || frag_spv.empty()) {
        spdlog::error(
            "ImagePipelineManager: shader compilation failed (empty SPIR-V)");
        return vk::Pipeline{};
    }

    // Create shader modules.
    auto create_shader_module =
        [&](const std::vector<uint32_t> &spv) -> vk::ShaderModule {
        vk::ShaderModuleCreateInfo ci;
        ci.setCodeSize(sizeof(uint32_t) * spv.size());
        ci.setPCode(spv.data());
        return _device.createShaderModule(ci);
    };

    vk::ShaderModule vert_module = create_shader_module(vert_spv);
    vk::ShaderModule frag_module = create_shader_module(frag_spv);

    // Shader stages.
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

    // ── Vertex input: EMPTY (fullscreen triangle, no vertex buffer) ──

    vk::PipelineVertexInputStateCreateInfo vertex_input;
    // No binding or attribute descriptions.

    // ── Input assembly ──────────────────────────────────────────────

    vk::PipelineInputAssemblyStateCreateInfo input_assembly;
    input_assembly.setTopology(vk::PrimitiveTopology::eTriangleList);
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
    rasterization.setCullMode(vk::CullModeFlagBits::eNone);
    rasterization.setFrontFace(vk::FrontFace::eCounterClockwise);
    rasterization.setDepthBiasEnable(VK_FALSE);
    rasterization.setLineWidth(1.0f);

    // ── Multisample ─────────────────────────────────────────────────

    vk::PipelineMultisampleStateCreateInfo multisampling;
    multisampling.setRasterizationSamples(
        static_cast<vk::SampleCountFlagBits>(_cached_msaa_samples));
    multisampling.setSampleShadingEnable(VK_FALSE);
    multisampling.setMinSampleShading(1.0f);

    // ── Depth / stencil ─────────────────────────────────────────────

    vk::PipelineDepthStencilStateCreateInfo depth_stencil;
    depth_stencil.setDepthTestEnable(VK_FALSE);
    depth_stencil.setDepthWriteEnable(VK_FALSE);
    depth_stencil.setStencilTestEnable(VK_FALSE);

    // ── Color blending ──────────────────────────────────────────────

    vk::PipelineColorBlendAttachmentState color_blend_attachment;
    color_blend_attachment.setColorWriteMask(
        vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG
        | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA);
    color_blend_attachment.setBlendEnable(VK_TRUE);
    color_blend_attachment.setSrcColorBlendFactor(vk::BlendFactor::eSrcAlpha);
    color_blend_attachment.setDstColorBlendFactor(
        vk::BlendFactor::eOneMinusSrcAlpha);
    color_blend_attachment.setColorBlendOp(vk::BlendOp::eAdd);
    color_blend_attachment.setSrcAlphaBlendFactor(vk::BlendFactor::eOne);
    color_blend_attachment.setDstAlphaBlendFactor(vk::BlendFactor::eZero);
    color_blend_attachment.setAlphaBlendOp(vk::BlendOp::eAdd);

    vk::PipelineColorBlendStateCreateInfo color_blend;
    color_blend.setLogicOpEnable(VK_FALSE);
    color_blend.setAttachmentCount(1);
    color_blend.setPAttachments(&color_blend_attachment);

    // ── Dynamic state ───────────────────────────────────────────────

    std::array<vk::DynamicState, 2> dynamic_states = {
        vk::DynamicState::eViewport,
        vk::DynamicState::eScissor,
    };
    vk::PipelineDynamicStateCreateInfo dynamic_state;
    dynamic_state.setDynamicStates(dynamic_states);

    // ── Assemble ────────────────────────────────────────────────────

    vk::RenderPass rp{reinterpret_cast<VkRenderPass>(_cached_render_pass)};

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
        spdlog::info("ImagePipelineManager: pipeline created successfully");
    } else {
        spdlog::error(
            "ImagePipelineManager: failed to create graphics pipeline");
    }

    // Cleanup shader modules.
    _device.destroyShaderModule(vert_module);
    _device.destroyShaderModule(frag_module);

    return result;
}

} // namespace balsa::visualization::vulkan
