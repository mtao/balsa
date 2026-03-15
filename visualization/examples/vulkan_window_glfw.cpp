#include <spdlog/spdlog.h>
#include <iostream>
#include <GLFW/glfw3.h>
#include "vulkan_scene.hpp"
#include <balsa/visualization/glfw/vulkan/window.hpp>

using namespace balsa::visualization;

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    glfwInit();

    try {
        glfw::vulkan::Window window("Hello Triangle + ImGui (GLFW)");

        auto scene = std::make_shared<BasicImGuiScene>();

        // Initialize ImGui while we have access to both Film and GLFWwindow.
        // Film is already initialized (render pass exists) at this point.
        scene->init_imgui(window.film(), window.glfw_window());

        window.set_scene(scene);
        scene.reset();// Window is sole owner — ensures scene is destroyed
                      // during window teardown while the device is still valid.

        return window.exec();

    } catch (const std::exception &e) {
        std::cerr << e.what() << std::endl;
        glfwTerminate();
        return EXIT_FAILURE;
    }

    glfwTerminate();
    return 0;
}
