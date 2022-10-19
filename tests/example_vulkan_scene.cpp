#if defined(SKIP_ALL)
#else
#include <spdlog/spdlog.h>
#include <balsa/visualization/shaders/flat.hpp>
#include "example_vulkan_scene.hpp"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#include <colormap/colormap.h>
#pragma GCC diagnostic pop
namespace balsa::visualization::shaders {

template<scene_graph::concepts::embedding_traits ET>
class TriangleShader : public Shader<ET> {
  public:
    TriangleShader() {}
    std::vector<uint32_t> vert_spirv() const override final;
    std::vector<uint32_t> frag_spirv() const override final;
};

template<scene_graph::concepts::embedding_traits ET>
std::vector<uint32_t> TriangleShader<ET>::vert_spirv() const {
    const static std::string fname = ":/glsl/triangle.vert";
    return AbstractShader::compile_glsl_from_path(fname, AbstractShader::ShaderType::Vertex);
}
template<scene_graph::concepts::embedding_traits ET>
std::vector<uint32_t> TriangleShader<ET>::frag_spirv() const {
    const static std::string fname = ":/glsl/triangle.frag";
    return AbstractShader::compile_glsl_from_path(fname, AbstractShader::ShaderType::Fragment);
}
}// namespace balsa::visualization::shaders

HelloTriangleScene::HelloTriangleScene() {}
// HelloTriangleScene::HelloTriangleScene(balsa::visualization::vulkan::Film &film)
HelloTriangleScene::~HelloTriangleScene() {
    if (device) {
        if (pipeline) {
            device.destroyPipeline(pipeline);
        }
        if (pipeline_layout) {
            device.destroyPipelineLayout(pipeline_layout);
        }
    }
}
    void HelloTriangleScene::initialize(balsa::visualization::vulkan::Film& film)
{
    if (!bool(device)) {
        device = film.device();
    }
    if (!bool(pipeline)) {
        create_graphics_pipeline(film);
    }


}

void HelloTriangleScene::begin_render_pass(balsa::visualization::vulkan::Film& film) 
{
    initialize(film);
    SceneBase::begin_render_pass(film);
}

void HelloTriangleScene::draw(balsa::visualization::vulkan::Film &film) {

    assert(film.device() == device);

    static float value = 0.0;
    value += 0.0005f;
    if (value > 1.0f)
        value = 0.0f;
    auto col = colormap::transform::LavaWaves().getColor(value);
    set_clear_color(float(col.r), float(col.g), float(col.b));
    begin_render_pass(film);

    auto cb = film.current_command_buffer();
    cb.bindPipeline(vk::PipelineBindPoint::eGraphics,
                    pipeline);

    vk::Viewport viewport{};
    auto extent = film.swapchain_image_size();

    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = (float)extent.x;
    viewport.height = (float)extent.y;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    cb.setViewport(0, { viewport });


    vk::Rect2D scissor{};
    scissor.offset = vk::Offset2D{ 0, 0 };
    scissor.extent = vk::Extent2D{ extent.x, extent.y };

    cb.setScissor(0, { scissor });


    cb.draw(3, 1, 0, 0);
    end_render_pass(film);
}


void HelloTriangleScene::create_graphics_pipeline(balsa::visualization::vulkan::Film &film) {
    spdlog::info("HelloTriangleScene: starting to build piepline");
    // balsa::visualization::shaders::FlatShader<balsa::scene_graph::embedding_traits2D> fs;
    balsa::visualization::shaders::TriangleShader<balsa::scene_graph::embedding_traits2D> fs;
    auto vert_spv = fs.vert_spirv();
    auto frag_spv = fs.frag_spirv();
    auto create_shader_module =
      [&](const std::vector<uint32_t> &spv) -> vk::ShaderModule {
        vk::ShaderModuleCreateInfo ci;
        ci.setCodeSize(sizeof(uint32_t) * spv.size());
        ci.setPCode(spv.data());
        return device.createShaderModule(ci);
    };

    vk::ShaderModule vert_shader_module = create_shader_module(vert_spv);
    vk::ShaderModule frag_shader_module = create_shader_module(frag_spv);
    vk::PipelineShaderStageCreateInfo shader_stages[2];

    const std::string name = "main";

    {
        auto &vert_stage = shader_stages[0];
        vert_stage.setStage(vk::ShaderStageFlagBits::eVertex);
        vert_stage.setModule(vert_shader_module);
        vert_stage.setPName(name.c_str());

        auto &frag_stage = shader_stages[1];
        frag_stage.setStage(vk::ShaderStageFlagBits::eFragment);
        frag_stage.setModule(frag_shader_module);
        frag_stage.setPName(name.c_str());
    }


    {
        vk::PipelineLayoutCreateInfo ci;
        ci.setSetLayoutCount(
          0);// automatic bindings have cool names, but why
        ci.setPushConstantRangeCount(0);
        pipeline_layout = device.createPipelineLayout(ci);
    }

    vk::PipelineVertexInputStateCreateInfo vertex_input;
    {
        auto &ci = vertex_input;
        ci.setVertexBindingDescriptionCount(0);
        ci.setVertexBindingDescriptions(nullptr);
        ci.setVertexAttributeDescriptionCount(0);
        ci.setVertexAttributeDescriptions(nullptr);
    }

    vk::PipelineInputAssemblyStateCreateInfo input_assembly;
    {
        auto &ci = input_assembly;
        ci.setTopology(vk::PrimitiveTopology::eTriangleList);
        // allow for triangle strip type tools with special reset indices
        ci.setPrimitiveRestartEnable(VK_FALSE);
    }
    auto ssize = film.swapchain_image_size();
    vk::Extent2D swapchain_extent(ssize.x, ssize.y);
    vk::Rect2D scissor;
    {
        scissor.setOffset({ 0, 0 });
        scissor.setExtent(swapchain_extent);
    }

    vk::Viewport viewport;
    {
        viewport.setX(0.0f);
        viewport.setY(0.0f);
        viewport.setWidth(swapchain_extent.width);
        viewport.setHeight(swapchain_extent.height);
        viewport.setMinDepth(0.0f);
        viewport.setMaxDepth(1.0f);
    }
    vk::PipelineViewportStateCreateInfo viewport_state;
    {
        auto &ci = viewport_state;
        ci.setViewportCount(1);
        ci.setPViewports(&viewport);
        ci.setScissorCount(1);
        ci.setPScissors(&scissor);
    }

    vk::PipelineRasterizationStateCreateInfo rasterization;
    {
        auto &ci = rasterization;
        ci.setDepthClampEnable(VK_FALSE);
        ci.setRasterizerDiscardEnable(VK_FALSE);
        ci.setLineWidth(1.0f);
        ci.setPolygonMode(vk::PolygonMode::eFill);
        ci.setCullMode(vk::CullModeFlagBits::eBack);
        ci.setFrontFace(vk::FrontFace::eClockwise);

        ci.setDepthBiasEnable(VK_FALSE);
        ci.setDepthBiasConstantFactor(0.0f);
        ci.setDepthBiasClamp(0.0f);
        ci.setDepthBiasSlopeFactor(0.0f);
    }

    vk::PipelineMultisampleStateCreateInfo multisampling;
    {
        auto &ci = multisampling;
        ci.setSampleShadingEnable(VK_FALSE);
        ci.setRasterizationSamples(vk::SampleCountFlagBits::e1);
        ci.setMinSampleShading(1.0f);
        ci.setPSampleMask(nullptr);
        ci.setAlphaToCoverageEnable(VK_FALSE);
        ci.setAlphaToOneEnable(VK_FALSE);
    }

    vk::PipelineColorBlendAttachmentState color_blend_attachment;
    {
        auto &s = color_blend_attachment;
        s.setColorWriteMask(vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA);
        s.setBlendEnable(VK_FALSE);
        s.setSrcColorBlendFactor(vk::BlendFactor::eOne);
        s.setDstColorBlendFactor(vk::BlendFactor::eZero);

        s.setColorBlendOp(vk::BlendOp::eAdd);
        s.setSrcAlphaBlendFactor(vk::BlendFactor::eOne);
        s.setDstAlphaBlendFactor(vk::BlendFactor::eZero);
        s.setAlphaBlendOp(vk::BlendOp::eAdd);
    }

    vk::PipelineColorBlendStateCreateInfo color_blend;
    {
        auto &ci = color_blend;
        ci.setLogicOpEnable(VK_FALSE);
        ci.setLogicOp(vk::LogicOp::eCopy);
        ci.setAttachmentCount(1);
        ci.setPAttachments(&color_blend_attachment);
        ci.setBlendConstants({ 0.f, 0.f, 0.f, 0.f });
    }

    std::vector<vk::DynamicState> dynamic_states = {
        vk::DynamicState::eViewport, vk::DynamicState::eLineWidth
    };

    vk::PipelineDynamicStateCreateInfo dynamic_state;
    {
        dynamic_state.setDynamicStateCount(dynamic_states.size());
        dynamic_state.setPDynamicStates(dynamic_states.data());
    }

    {
        vk::GraphicsPipelineCreateInfo pipeline_info;
        auto &ci = pipeline_info;

        ci.setStageCount(2);
        ci.setPStages(shader_stages);
        ci.setPVertexInputState(&vertex_input);
        ci.setPInputAssemblyState(&input_assembly);
        ci.setPViewportState(&viewport_state);
        ci.setPRasterizationState(&rasterization);
        ci.setPMultisampleState(&multisampling);
        ci.setPColorBlendState(&color_blend);
        ci.setPDynamicState(&dynamic_state);
        ci.setLayout(pipeline_layout);
        ci.setRenderPass(film.default_render_pass());
        ci.setSubpass(0);
        ci.setBasePipelineHandle(VK_NULL_HANDLE);

        auto res = device.createGraphicsPipeline(VK_NULL_HANDLE, pipeline_info);
        if (res.result == vk::Result::eSuccess) {
            pipeline = res.value;
        } else {
            spdlog::error("was unable to create graphics pipeline");
        }
    }


    device.destroyShaderModule(vert_shader_module);
    device.destroyShaderModule(frag_shader_module);
    spdlog::info("HelloTriangleScene: Finished building pipeline");
}

#endif
