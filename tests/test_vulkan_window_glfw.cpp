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
            glfwPollEvents();
            film->pre_draw();
            drawFrame();
            film->post_draw();
            film->device().waitIdle();
        }
    }


    void record_command_buffer(vk::raii::CommandBuffer &cb) {
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
            rpi.setFramebuffer(film->currentFramebuffer());
            rpi.renderArea.setOffset({ 0, 0 });
            auto extent = film->swapChainImageSize();
            rpi.renderArea.setExtent({ extent.x, extent.y });

            rpi.setClearValueCount(1);
            rpi.setPClearValues(&clear_color);
        }

        cb.beginRenderPass(rpi, vk::SubpassContents::eInline);

        cb.bindPipeline(vk::PipelineBindPoint::eGraphics,
                        PIPELINE);

        cb.draw(3, 1, 0, 0);

        cb.endRenderPass();

        cb.end();
    }

    void draw_frame() {
        record_command_buffer(command_buffer, image_index);
    }
};


//! [0]
int main(int argc, char *argv[]) {
    QGuiApplication app(argc, argv);


    std::vector<std::string> validation_layers = {
        "VK_LAYER_KHRONOS_validation"
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

    using embedding_traits = balsa::scene_graph::embedding_traits3F;

    balsa::visualization::shaders::FlatShader<embedding_traits> fs;
    spdlog::info("Vertex shader");
    auto vdata = fs.vert_spirv();
    spdlog::info("Fragment shader");
    auto fdata = fs.frag_spirv();
    std::cout << vdata.size() << " " << fdata.size() << std::endl;
    // fs.make_shader();
    //! [1]

    return app.exec();
}
