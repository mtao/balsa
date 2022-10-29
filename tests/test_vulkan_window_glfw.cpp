#include <span>
#include <spdlog/spdlog.h>
#include <iostream>
#include <GLFW/glfw3.h>
#include "example_vulkan_scene.hpp"
#include <balsa/visualization/glfw//vulkan//Film.hpp>
#include <balsa/visualization/glfw//vulkan//Window.hpp>

#include <spdlog/cfg/env.h>

using namespace balsa::visualization;

class TestWindow : public balsa::visualization::glfw::vulkan::Window {
  public:
    TestWindow() : balsa::visualization::glfw::vulkan::Window("Hello", 800, 600, false) {

        std::vector<std::string> validation_layers = {
            "VK_LAYER_KHRONOS_validation",
        };
        native_film().set_validation_layers(validation_layers);

        native_film().initialize();
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
    spdlog::cfg::load_env_levels();
    glfwInit();

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
