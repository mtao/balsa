#include <balsa/scene_graph/embedding_traits.hpp>
#include <span>
#include <QGuiApplication>
#include <spdlog/spdlog.h>
#include <iostream>
#include <colormap/colormap.h>
#include <balsa/visualization/shaders/flat.hpp>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>
#include <balsa/visualization/vulkan/scene.hpp>
#include <balsa/scene_graph/transformations/matrix_transformation.hpp>
#include <balsa/visualization/glfw//vulkan//film.hpp>
#include <GLFW/glfw3.h>

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

class Scene : public balsa::visualization::vulkan::Scene<balsa::scene_graph::transformations::MatrixTransformation<balsa::scene_graph::embedding_traits3F>> {
    using embedding_traits = balsa::scene_graph::embedding_traits3F;
    using transformation_type = balsa::scene_graph::transformations::MatrixTransformation<embedding_traits>;
    using scene_type = balsa::visualization::vulkan::Scene<transformation_type>;

  public:
    Scene(balsa::visualization::vulkan::Film &film) : device(film.device()) {
        create_graphics_pipeline(film);
    }
    ~Scene() {
        device.destroyPipeline(pipeline);
        device.destroyPipelineLayout(pipeline_layout);
    }


    void draw(balsa::visualization::vulkan::Film &film) {
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


  private:
    void create_graphics_pipeline(balsa::visualization::vulkan::Film &film) {
        // balsa::visualization::shaders::FlatShader<balsa::scene_graph::embedding_traits2D> fs;
        balsa::visualization::shaders::TriangleShader<balsa::scene_graph::embedding_traits2D> fs;
        auto vert_spv = fs.vert_spirv();
        auto frag_spv = fs.frag_spirv();
        auto create_shader_module =
          [&](const std::vector<uint32_t> &spv) -> vk::ShaderModule {
            vk::ShaderModuleCreateInfo ci;
            ci.setCodeSize(sizeof(uint32_t) * spv.size());
            ci.setPCode(spv.data());
            return film.device().createShaderModule(ci);
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
            pipeline_layout = film.device().createPipelineLayout(ci);
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

            auto res = film.device().createGraphicsPipeline(VK_NULL_HANDLE, pipeline_info);
            if (res.result == vk::Result::eSuccess) {
                pipeline = res.value;
            } else {
                spdlog::error("was unable to create graphics pipeline");
            }
        }


        device.destroyShaderModule(vert_shader_module);
        device.destroyShaderModule(frag_shader_module);
    }

    vk::Device device;
    vk::PipelineLayout pipeline_layout = nullptr;
    vk::Pipeline pipeline = nullptr;
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
    HelloTriangleApplication() : window(make_window()),
                                 film(std::make_unique<GLFWFilmTestShim>(window)),
                                 scene(*film) {
        // debugMessenger = *film->_debug_messenger_raii;
        // physicalDevice = *film->physical_device();
        // device = *film->device();
    }

    void run() { main_loop(); }
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


    void main_loop() {
        while (!glfwWindowShouldClose(window)) {
            spdlog::info("Doing a frame");
            glfwPollEvents();
            film->pre_draw();
            draw_frame();
            film->post_draw();
            film->device().waitIdle();
        }
    }


    void draw_frame() {
        scene.draw(*film);
    }
};


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
