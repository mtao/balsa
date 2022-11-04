#if !defined(BALSA_VISUALIZATION_VULKAN_UTILS_PIPELINE_FACTORY_HPP)
#define BALSA_VISUALIZATION_VULKAN_UTILS_PIPELINE_FACTORY_HPP

#include <vulkan/vulkan.hpp>
#include <span>


namespace balsa::visualization {
namespace shaders {
    class AbstractShader;
}
namespace vulkan {
    class Film;
    class VertexBufferView;
    namespace utils {
        class VertexBufferViewCollection;

        class PipelineFactory {
          public:
            PipelineFactory(Film &film);
            ~PipelineFactory();
            std::tuple<vk::Pipeline, vk::PipelineLayout> generate();

            vk::PipelineVertexInputStateCreateInfo vertex_input;
            // Helpers for letting users construct vertex_input
            void set_vertex_inputs(const VertexBufferViewCollection &collection);
            void add_vertex_input(const VertexBufferView &view);
            void set_vertex_input(const VertexBufferView &view);
            void clear_vertex_inputs();


            vk::PipelineInputAssemblyStateCreateInfo input_assembly;
            vk::PipelineViewportStateCreateInfo viewport_state;
            vk::PipelineRasterizationStateCreateInfo rasterization_state;
            vk::PipelineMultisampleStateCreateInfo multisampling_state;
            vk::PipelineColorBlendAttachmentState color_blend_attachment;
            vk::PipelineColorBlendStateCreateInfo color_blend_state;
            vk::PipelineDynamicStateCreateInfo dynamic_state;
            vk::GraphicsPipelineCreateInfo graphics_pipeline;

            vk::PipelineLayoutCreateInfo pipeline_layout;
            // Helpers for letting users construct vertex_input
            // Note that generate() is responsible for doing the final assignment
            void add_descriptor_set_layout(const shaders::AbstractShader &shader);
            void build_shaders(const shaders::AbstractShader &shader);

            vk::Viewport viewport;
            vk::Extent2D swapchain_extent;
            vk::Rect2D scissor;
            std::vector<vk::DynamicState> dynamic_states;


          private:
            vk::Device m_device;
            std::vector<vk::VertexInputBindingDescription> m_binding_descriptions;
            std::vector<vk::VertexInputAttributeDescription> m_attribute_descriptions;
            class ShaderCollection;
            // a little pimpling!
            std::unique_ptr<ShaderCollection> m_shaders;
            class DescriptorSetLayoutCollection;
            // a little pimpling!
            std::unique_ptr<DescriptorSetLayoutCollection> m_descriptor_set_layouts;
        };
    }// namespace utils
}// namespace vulkan
}// namespace balsa::visualization

#endif
