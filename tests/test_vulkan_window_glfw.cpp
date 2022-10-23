#include <span>
#include <spdlog/spdlog.h>
#include <iostream>
#include <GLFW/glfw3.h>
#include "example_vulkan_scene.hpp"
#include <balsa/visualization/glfw//vulkan//film.hpp>
#include <balsa/visualization/glfw//vulkan//window.hpp>


using namespace balsa::visualization;



int main(int argc, char *argv[]) {
    glfwInit();

    std::vector<std::string> validation_layers = {
        "VK_LAYER_KHRONOS_validation",
        "VK_LAYER_LUNARG_parameter_validation",
        "VK_LAYER_LUNARG_core_validation",
    };
    //! [0]

    //spdlog::set_level(spdlog::level::trace);
    int retvalue = 0;
    try {
        balsa::visualization::glfw::vulkan::Window window("Hello");

        auto scene = std::make_shared<HelloTriangleScene>();

        window.set_scene(scene);

        retvalue = window.exec();


    } catch (const std::exception &e) {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }
    glfwTerminate();

    // fs.make_shader();
    //! [1]

    return retvalue;
}
