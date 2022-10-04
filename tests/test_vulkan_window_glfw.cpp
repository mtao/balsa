#include <balsa/scene_graph/embedding_traits.hpp>
#include <QGuiApplication>
#include <spdlog/spdlog.h>
#include <iostream>
#include <colormap/colormap.h>
#include <balsa/visualization/shaders/flat.hpp>
#include <vulkan/vulkan.hpp>
#include <balsa/visualization/vulkan/scene.hpp>
#include <balsa/scene_graph/transformations/matrix_transformation.hpp>
#include <balsa/visualization/glfw//vulkan//film.hpp>
#include <GLFW/glfw3.h>

class Scene : public balsa::visualization::vulkan::Scene<balsa::scene_graph::transformations::MatrixTransformation<balsa::scene_graph::embedding_traits3F>> {
    using embedding_traits = balsa::scene_graph::embedding_traits3F;
    using transformation_type = balsa::scene_graph::transformations::MatrixTransformation<embedding_traits>;
    using scene_type = balsa::visualization::vulkan::Scene<transformation_type>;

  public:
    void draw(const camera_type &, balsa::visualization::vulkan::Film &film) {

        static float value = 0.0;
        value += 0.0005f;
        if (value > 1.0f)
            value = 0.0f;
        auto col = colormap::transform::LavaWaves().getColor(value);
        set_clear_color(float(col.r), float(col.g), float(col.b));
        draw_background(film);
    }
};

using namespace balsa::visualization;


// this shim proves why protected is stupid
// but maybe it's actually really cool - testing shims only add a little bit of
// work for writing tests, but not so much that writing tests feels burdensome
class GLFWFilmTestShim : public glfw::vulkan::Film {
  public:
    using Film::Film;
    using Film::swapchain;
};


class HelloTriangleApplication {
  public:
    HelloTriangleApplication() : window(make_window()) {
        film = std::make_unique<GLFWFilmTestShim>(window);
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
    std::unique_ptr<GLFWFilmTestShim> film;
    // std::unique_ptr<glfw::vulkan::Film> film;
    Scene scene;


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
            old_draw_frame();
            // film->pre_draw();
            // draw_frame();
            // film->post_draw();
            //  film->device().waitIdle();
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
            auto extent = film->swapchain_image_size();
            rpi.renderArea.setExtent({ extent.x, extent.y });

            rpi.setClearValueCount(1);
            rpi.setPClearValues(&clear_color);
        }

        cb.beginRenderPass(rpi, vk::SubpassContents::eInline);

        cb.bindPipeline(vk::PipelineBindPoint::eGraphics,
                        *pipeline);

        cb.draw(3, 1, 0, 0);
    }
    void old_record_command_buffer() {
        auto cb = film->current_command_buffer();
        vk::CommandBufferBeginInfo bi;
        {
            bi.setFlags({});// not used
            bi.setPInheritanceInfo(nullptr);
            cb.begin(bi);
        }

        vk::ClearValue clear_color =
          vk::ClearColorValue{ std::array<float, 4>{ 0.f, 0.f, 0.f, 1.f } };
        vk::RenderPassBeginInfo rpi;
        {
            rpi.setRenderPass(film->default_render_pass());
            rpi.setFramebuffer(film->current_framebuffer());
            rpi.renderArea.setOffset({ 0, 0 });
            auto extent = film->swapchain_image_size();
            rpi.renderArea.setExtent({ extent.x, extent.y });

            rpi.setClearValueCount(1);
            rpi.setPClearValues(&clear_color);
        }

        cb.beginRenderPass(rpi, vk::SubpassContents::eInline);

        cb.bindPipeline(vk::PipelineBindPoint::eGraphics,
                        *pipeline);

        cb.draw(3, 1, 0, 0);

        cb.endRenderPass();

        cb.end();
    }

    void old_draw_frame() {
        auto device = film->device();
        const auto &frame = film->frame_resources();

        vk::Result result;
        result = device.waitForFences(*frame.fence_raii, VK_TRUE, UINT64_MAX);
        if (result != vk::Result::eSuccess) {
            spdlog::error("Fence failed");
        }
        device.resetFences(*frame.fence_raii);

        uint32_t image_index;
        result = device.acquireNextImageKHR(film->swapchain(), UINT64_MAX, *frame.image_semaphore_raii, VK_NULL_HANDLE, &image_index);
        if (result != vk::Result::eSuccess) {
            spdlog::error("Failed to acquire image");
        }
        auto command_buffer = film->current_command_buffer();

        record_command_buffer();

        vk::SubmitInfo si;
        vk::PipelineStageFlags wait_stages[] = {
            vk::PipelineStageFlagBits::eColorAttachmentOutput
        };
        {
            si.setWaitSemaphores(*frame.image_semaphore_raii);

            si.setWaitDstStageMask(*wait_stages);
            si.setCommandBuffers(command_buffer);
            si.setSignalSemaphores(*frame.draw_semaphore_raii);
        }

        film->graphics_queue().submit(si, *frame.fence_raii);

        vk::PresentInfoKHR present;
        present.setWaitSemaphores(*frame.draw_semaphore_raii);

        present.setSwapchains(film->swapchain());
        present.setImageIndices(image_index);

        present.setPResults(nullptr);
        result = film->present_queue().presentKHR(present);
        if (result != vk::Result::eSuccess) {
            spdlog::error("Failed to present");
        }
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

            auto ssize = film->swapchain_image_size();
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