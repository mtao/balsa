#include "balsa/visualization/glfw/vulkan/Window.hpp"
#include "balsa/visualization/glfw/vulkan/Film.hpp"
#include <GLFW/glfw3.h>
#include <spdlog/spdlog.h>

namespace balsa::visualization::glfw::vulkan {
Window::Window(const std::string_view &title, int width, int height) : GLFWParentType(title, width, height),
                                                                       VulkanParentType(),
                                                                       m_film(GLFWParentType::window()) {
}


void Window::pre_draw_hook() {
    GLFWParentType::pre_draw_hook();
    film().pre_draw_hook();
    VulkanParentType::pre_draw_hook();
}

void Window::post_draw_hook() {
    VulkanParentType::post_draw_hook();
    film().post_draw_hook();
    film().device().waitIdle();
    GLFWParentType::post_draw_hook();
}
int Window::exec() {
    const auto &scene = this->scene();
    if (scene) {
        scene->initialize(film());
    }
    GLFWParentType::main_loop();
    return 0;
}

void Window::draw_frame() {
    VulkanParentType::draw_frame();
}
void Window::window_size(int w, int h) {
    resize(w, h);
}

void Window::resize(int, int) {
    m_film.framebuffer_was_resized();
}

visualization::vulkan::Film &Window::film() {
    return m_film;
}

}// namespace balsa::visualization::glfw::vulkan
