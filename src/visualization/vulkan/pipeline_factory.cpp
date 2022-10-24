#include "balsa/visualization/vulkan/pipeline_factory.hpp"
#include "balsa/visualization/vulkan/film.hpp"
#include "balsa/visualization/shaders/abstract_shader.hpp"
#include <spdlog/spdlog.h>
#include <range/v3/view/transform.hpp>
#include <range/v3/view/zip.hpp>
#include <range/v3/range/conversion.hpp>


namespace balsa::visualization::vulkan {
PipelineFactory::PipelineFactory(Film &film) {

    {
        auto &ci = vertex_input;
        ci.setVertexBindingDescriptionCount(0);
        ci.setVertexBindingDescriptions(nullptr);
        ci.setVertexAttributeDescriptionCount(0);
        ci.setVertexAttributeDescriptions(nullptr);
    }

    {
        auto &ci = input_assembly;
        ci.setTopology(vk::PrimitiveTopology::eTriangleList);
        // allow for triangle strip type tools with special reset indices
        ci.setPrimitiveRestartEnable(VK_FALSE);
    }
    auto ssize = film.swapchain_image_size();
    swapchain_extent = vk::Extent2D(ssize.x, ssize.y);
    {
        scissor.setOffset({ 0, 0 });
        scissor.setExtent(swapchain_extent);
    }

    {
        viewport.setX(0.0f);
        viewport.setY(0.0f);
        viewport.setWidth(swapchain_extent.width);
        viewport.setHeight(swapchain_extent.height);
        viewport.setMinDepth(0.0f);
        viewport.setMaxDepth(1.0f);
    }
    {
        auto &ci = viewport_state;
        ci.setViewportCount(1);
        ci.setPViewports(&viewport);
        ci.setScissorCount(1);
        ci.setPScissors(&scissor);
    }

    {
        auto &ci = rasterization_state;
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

    {
        auto &ci = multisampling_state;
        ci.setSampleShadingEnable(VK_FALSE);
        ci.setRasterizationSamples(vk::SampleCountFlagBits::e1);
        ci.setMinSampleShading(1.0f);
        ci.setPSampleMask(nullptr);
        ci.setAlphaToCoverageEnable(VK_FALSE);
        ci.setAlphaToOneEnable(VK_FALSE);
    }

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

    {
        auto &ci = color_blend_state;
        ci.setLogicOpEnable(VK_FALSE);
        ci.setLogicOp(vk::LogicOp::eCopy);
        ci.setAttachmentCount(1);
        ci.setPAttachments(&color_blend_attachment);
        ci.setBlendConstants({ 0.f, 0.f, 0.f, 0.f });
    }

    dynamic_states = {
        vk::DynamicState::eViewport, vk::DynamicState::eLineWidth
    };

    {
        dynamic_state.setDynamicStateCount(dynamic_states.size());
        dynamic_state.setPDynamicStates(dynamic_states.data());
    }

    {
        auto &ci = pipeline_layout;
        ci.setSetLayoutCount(
          0);// automatic bindings have cool names, but why
        ci.setPushConstantRangeCount(0);
    }

    {
        auto &ci = graphics_pipeline;

        ci.setStageCount(2);
        ci.setPVertexInputState(&vertex_input);
        ci.setPInputAssemblyState(&input_assembly);
        ci.setPViewportState(&viewport_state);
        ci.setPRasterizationState(&rasterization_state);
        ci.setPMultisampleState(&multisampling_state);
        ci.setPColorBlendState(&color_blend_state);
        ci.setPDynamicState(&dynamic_state);
        ci.setRenderPass(film.default_render_pass());
        ci.setSubpass(0);
        ci.setBasePipelineHandle(VK_NULL_HANDLE);
    }
}
std::tuple<vk::Pipeline, vk::PipelineLayout>
  PipelineFactory::generate(Film &film, shaders::AbstractShader &shader) {
    auto device = film.device();
    std::tuple<vk::Pipeline, vk::PipelineLayout> ret;
    auto &[pipeline, pipeline_layout] = ret;


    auto spirv = shader.compile_spirv();

    auto shader_to_module =
      [&device](const shaders::AbstractShader::SpirvShader &shader) -> vk::ShaderModule {
        const auto &spv = shader.spirv_data;
        vk::ShaderModuleCreateInfo ci;
        ci.setCodeSize(sizeof(uint32_t) * spv.size());
        ci.setPCode(spv.data());
        return device.createShaderModule(ci);
    };

    std::vector<vk::ShaderModule>
      shader_modules = spirv | ranges::views::transform(shader_to_module)
                       | ranges::to_vector;


    const std::string name = "main";

    auto balsa_shader_stage_to_vk_shader_stage = [](shaders::AbstractShader::ShaderStage stage) -> vk::ShaderStageFlagBits {
        switch (stage) {
        case shaders::AbstractShader::ShaderStage::Vertex:
            return vk::ShaderStageFlagBits::eVertex;
        case shaders::AbstractShader::ShaderStage::Fragment:
            return vk::ShaderStageFlagBits::eFragment;
        }
        return vk::ShaderStageFlagBits::eVertex;
    };
    auto zipped_to_stages = [&](const auto &pr) {
        vk::PipelineShaderStageCreateInfo stage;
        const auto &[spirv, shader_module] = pr;
        stage.setStage(balsa_shader_stage_to_vk_shader_stage(spirv.stage));
        stage.setModule(shader_module);
        stage.setPName(spirv.name.c_str());
        return stage;
    };

    std::vector<vk::PipelineShaderStageCreateInfo>
      shader_stages = ranges::views::zip(spirv, shader_modules) | ranges::views::transform(zipped_to_stages)
                      | ranges::to_vector;


    {
        auto &ci = this->pipeline_layout;
        pipeline_layout = device.createPipelineLayout(ci);
    }
    {
        auto &ci = graphics_pipeline;
        ci.setStages(shader_stages);
        ci.setLayout(pipeline_layout);
    }

    auto res = device.createGraphicsPipeline(VK_NULL_HANDLE, graphics_pipeline);
    if (res.result == vk::Result::eSuccess) {
        pipeline = res.value;
    } else {
        spdlog::error("was unable to create graphics pipeline");
    }
    for (const auto &mod : shader_modules) {
        device.destroyShaderModule(mod);
    }
    return ret;
}
}// namespace balsa::visualization::vulkan
