#include <balsa/scene_graph/embedding_traits.hpp>
#include <QGuiApplication>
#include <spdlog/spdlog.h>
#include <iostream>
#include <colormap/colormap.h>
#include <balsa/visualization/shaders/flat.hpp>
#include <balsa/visualization/glfw//vulkan//film.hpp>
#include <GLFW/glfw3.h>

using namespace balsa::visualization;
class HelloTriangleApplication {
  public:
    HelloTriangleApplication() : window(make_window()) {
        film = std::make_unique<glfw::vulkan::Film>(window);
        create_graphics_pipeline();
    }

    void run() { mainLoop(); }
    ~HelloTriangleApplication() {
        glfwDestroyWindow(window);
    }

  private:
    const uint32_t WIDTH = 800;
    const uint32_t HEIGHT = 600;
    GLFWwindow *window = nullptr;
    std::unique_ptr<glfw::vulkan::Film> film;


    vk::raii::PipelineLayout pipeline_layout = nullptr;
    vk::raii::Pipeline pipeline = nullptr;

    GLFWwindow *make_window() {
        // don't want to make glfw accidentally create an opengl context
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
        GLFWwindow *window =
          glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);

        const char *description;
        int code = glfwGetError(&description);
        if (code != GLFW_NO_ERROR) {
            spdlog::error("glfw init error: {}", description);
            throw std::runtime_error("Could not start GLFW");
        }
        return window;
    }

    void mainLoop() {
        while (!glfwWindowShouldClose(window)) {
            spdlog::info("Doing a frame");
            glfwPollEvents();
            film->pre_draw();
            draw_frame();
            film->post_draw();
            // film->device().waitIdle();
        }
    }


    void record_command_buffer() {
        auto cb = film->current_command_buffer();

        vk::ClearValue clear_color =
          vk::ClearColorValue{ std::array<float, 4>{ 0.f, 0.f, 0.f, 1.f } };
        vk::RenderPassBeginInfo rpi;
        {
            rpi.setRenderPass(film->default_render_pass());
            rpi.setFramebuffer(film->current_framebuffer());
            rpi.renderArea.setOffset({ 0, 0 });
            auto extent = film->swapChainImageSize();
            rpi.renderArea.setExtent({ extent.x, extent.y });

            rpi.setClearValueCount(1);
            rpi.setPClearValues(&clear_color);
        }

        cb.beginRenderPass(rpi, vk::SubpassContents::eInline);

        cb.bindPipeline(vk::PipelineBindPoint::eGraphics,
                        *pipeline);

        cb.draw(3, 1, 0, 0);
    }

    void draw_frame() {
        record_command_buffer();
    }
    void create_graphics_pipeline() {
        balsa::visualization::shaders::FlatShader<balsa::scene_graph::embedding_traits2D> fs;
        auto vert_spv = fs.vert_spirv();
        auto frag_spv = fs.frag_spirv();

        auto create_shader_module =
          [&](const std::vector<uint32_t> &spv) -> vk::raii::ShaderModule {
            vk::ShaderModuleCreateInfo ci;
            ci.setCodeSize(sizeof(uint32_t) * spv.size());
            ci.setPCode(spv.data());
            return film->device_raii().createShaderModule(ci);
        };

        vk::raii::ShaderModule vert_shader_module = create_shader_module(vert_spv);
        vk::raii::ShaderModule frag_shader_module = create_shader_module(frag_spv);
        vk::PipelineShaderStageCreateInfo shader_stages[2];

        const std::string name = "main";

        {
            auto &vert_stage = shader_stages[0];
            vert_stage.setStage(vk::ShaderStageFlagBits::eVertex);
            vert_stage.setModule(*vert_shader_module);
            vert_stage.setPName(name.c_str());

            auto &frag_stage = shader_stages[1];
            frag_stage.setStage(vk::ShaderStageFlagBits::eFragment);
            frag_stage.setModule(*frag_shader_module);
            frag_stage.setPName(name.c_str());
        }

        {
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

            auto ssize = film->swapChainImageSize();
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
                vk::PipelineLayoutCreateInfo ci;
                ci.setSetLayoutCount(
                  0);// automatic bindings have cool names, but why
                     // did vulkan not call this layoutCount
                ci.setPSetLayouts(nullptr);
                ci.setPushConstantRangeCount(0);
                ci.setPushConstantRanges(nullptr);
                pipeline_layout = film->device_raii().createPipelineLayout(ci);
            }

            vk::GraphicsPipelineCreateInfo pipeline_info;
            {
                auto &ci = pipeline_info;
                ci.setStageCount(2);
                ci.setPStages(shader_stages);
                ci.setPVertexInputState(&vertex_input);
                ci.setPInputAssemblyState(&input_assembly);
                ci.setPViewportState(&viewport_state);
                ci.setPRasterizationState(&rasterization);
                ci.setPMultisampleState(&multisampling);
                ci.setPDepthStencilState(nullptr);
                ci.setPColorBlendState(&color_blend);
                ci.setPDynamicState(nullptr);

                ci.setLayout(*pipeline_layout);
                ci.setRenderPass(film->default_render_pass());
                ci.setSubpass(0);

                ci.setBasePipelineHandle(VK_NULL_HANDLE);
                ci.setBasePipelineIndex(-1);
            }

            pipeline = film->device_raii().createGraphicsPipeline(VK_NULL_HANDLE, pipeline_info);
        }
    }
};


//! [0]
int main(int argc, char *argv[]) {

    glfwInit();

    std::vector<std::string> validation_layers = {
        "VK_LAYER_KHRONOS_validation",
        "VK_LAYER_LUNARG_parameter_validation",
        "VK_LAYER_LUNARG_core_validation",
    };
    //! [0]

    try {
        HelloTriangleApplication app;
        app.run();
    } catch (const std::exception &e) {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }
    glfwTerminate();

    // fs.make_shader();
    //! [1]

    return 0;
}
