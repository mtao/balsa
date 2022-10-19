#include <span>
#include <QGuiApplication>
#include <spdlog/spdlog.h>
#include <iostream>
#include <GLFW/glfw3.h>
#include "example_vulkan_scene.hpp"
#include <balsa/visualization/glfw//vulkan//film.hpp>


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
                                 scene() {
        // debugMessenger = *film->_debug_messenger_raii;
        // physicalDevice = *film->physical_device();
        // device = *film->device();
    }

    void run() { 
        scene.initialize(*film);
        main_loop(); 
    }
    ~HelloTriangleApplication() {
        glfwDestroyWindow(window);
    }

  private:
    const uint32_t WIDTH = 800;
    const uint32_t HEIGHT = 600;
    GLFWwindow *window = nullptr;
    std::unique_ptr<GLFWFilmTestShim> film;
    // std::unique_ptr<glfw::vulkan::Film> film;
    HelloTriangleScene scene;


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
