
#if !defined(BALSA_VISUALIZATION_VULKAN_PIPELINE_FACTORY_HPP)
#define BALSA_VISUALIZATION_VULKAN_PIPELINE_FACTORY_HPP

#include <vulkan/vulkan.hpp>


namespace balsa::visualization {
namespace shaders {
    class AbstractShader;
}
namespace vulkan {
    class Film;

    class PipelineFactory {
      public:
        PipelineFactory(Film &film);
        std::tuple<vk::Pipeline, vk::PipelineLayout> generate(Film &film, shaders::AbstractShader &shader);

        vk::PipelineVertexInputStateCreateInfo vertex_input;
        vk::PipelineInputAssemblyStateCreateInfo input_assembly;
        vk::PipelineViewportStateCreateInfo viewport_state;
        vk::PipelineRasterizationStateCreateInfo rasterization_state;
        vk::PipelineMultisampleStateCreateInfo multisampling_state;
        vk::PipelineColorBlendAttachmentState color_blend_attachment;
        vk::PipelineColorBlendStateCreateInfo color_blend_state;
        vk::PipelineDynamicStateCreateInfo dynamic_state;
        vk::GraphicsPipelineCreateInfo graphics_pipeline;

        vk::PipelineLayoutCreateInfo pipeline_layout;

        vk::Viewport viewport;
        vk::Extent2D swapchain_extent;
        vk::Rect2D scissor;
        std::vector<vk::DynamicState> dynamic_states;


      private:
    };
}// namespace vulkan
}// namespace balsa::visualization

#endif
