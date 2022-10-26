#include <span>
#include <spdlog/spdlog.h>
#include <iostream>
#include <GLFW/glfw3.h>
#include "example_vulkan_scene.hpp"
#include <balsa/visualization/glfw//vulkan//film.hpp>
#include <balsa/visualization/glfw//vulkan//window.hpp>


using namespace balsa::visualization;

class TestWindow : public balsa::visualization::glfw::vulkan::Window {
  public:
    TestWindow() : balsa::visualization::glfw::vulkan::Window("Hello") {
        m_scene = std::make_shared<HelloTriangleScene>();
    }
    balsa::visualization::vulkan::SceneBase *scene() const override {
        return m_scene.get();
    }

    void
      key(int key, int scancode, int action, int mods) override {
        if (key == GLFW_KEY_SPACE && action == GLFW_PRESS) {
            m_scene->toggle_mode(film());
        }
    }
    ~TestWindow() {
        m_scene = {};
        spdlog::info("Deleting scene done");
    }
    std::shared_ptr<HelloTriangleScene> m_scene;
};

int main(int argc, char *argv[]) {
    glfwInit();

    std::vector<std::string> validation_layers = {
        "VK_LAYER_KHRONOS_validation",
        "VK_LAYER_LUNARG_parameter_validation",
        "VK_LAYER_LUNARG_core_validation",
    };
    //! [0]

    // spdlog::set_level(spdlog::level::trace);
    int retvalue = 0;
    try {
        TestWindow window;


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
