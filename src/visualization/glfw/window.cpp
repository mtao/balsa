#if !defined(BALSA_VISUALIZATION_GLFW_VULKAN_WINDOW_HPP)
#define BALSA_VISUALIZATION_GLFW_VULKAN_WINDOW_HPP

#include <GLFW/glfw3.h>
#include "balsa/visualization/glfw/window.hpp"
#include <spdlog/spdlog.h>
#include <mutex>
#include <map>


class GLFWwindow;
namespace balsa::visualization::glfw {

namespace {

    // GLFW callbacks are just functions so we need to identfiy which window each callback calls from
    std::mutex active_window_registry_mutex;
    std::map<GLFWwindow *, Window *> active_window_registry;
    Window *get_window_from_registry(GLFWwindow *window) {
        std::scoped_lock sl(active_window_registry_mutex);
        if (auto it = active_window_registry.find(window); it != active_window_registry.end()) {
            return it->second;
        } else {
            spdlog::error("Could now find window for callback");
        }
        return nullptr;
    }
}// namespace


Window::Window(const std::string_view &title, int width, int height) {

    // don't want to make glfw accidentally create an opengl context
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    m_window =
      glfwCreateWindow(width, height, title.data(), nullptr, nullptr);

    const char *description;
    int code = glfwGetError(&description);
    if (code != GLFW_NO_ERROR) {
        spdlog::error("glfw init error: {}", description);
        throw std::runtime_error("Could not start GLFW");
    }
    {
        std::scoped_lock sl(active_window_registry_mutex);
        active_window_registry[m_window] = this;
    }
    initialize_glfw_callbacks();
}

Window::~Window() {
    if (auto it = active_window_registry.find(m_window); it != active_window_registry.end()) {
        std::scoped_lock sl(active_window_registry_mutex);
        active_window_registry.erase(it);
    }
    if (m_window) {
        glfwDestroyWindow(m_window);
    }
}


void Window::initialize_glfw_callbacks() {
    spdlog::info("Initializing callbacks");
    glfwSetWindowPosCallback(m_window, Window::setWindowPos_callback);
    glfwSetWindowSizeCallback(m_window, Window::setWindowSize_callback);
    glfwSetWindowCloseCallback(m_window, Window::setWindowClose_callback);
    glfwSetWindowRefreshCallback(m_window, Window::setWindowRefresh_callback);
    glfwSetWindowFocusCallback(m_window, Window::setWindowFocus_callback);
    glfwSetWindowIconifyCallback(m_window, Window::setWindowIconify_callback);
    glfwSetWindowMaximizeCallback(m_window, Window::setWindowMaximize_callback);
    glfwSetFramebufferSizeCallback(m_window, Window::setFramebufferSize_callback);
    glfwSetWindowContentScaleCallback(m_window, Window::setWindowContentScale_callback);
}

void Window::setWindowPos_callback(GLFWwindow *window, int xpos, int ypos) {
    Window *mywin = get_window_from_registry(window);
    if (mywin != nullptr) { mywin->set_window_pos(xpos, ypos); }
}
void Window::setWindowSize_callback(GLFWwindow *window, int width, int height) {
    Window *mywin = get_window_from_registry(window);
    if (mywin != nullptr) { mywin->set_window_size(width, height); }
}
void Window::setWindowClose_callback(GLFWwindow *window) {
    Window *mywin = get_window_from_registry(window);
    if (mywin != nullptr) { mywin->set_window_close(); }
}
void Window::setWindowRefresh_callback(GLFWwindow *window) {
    Window *mywin = get_window_from_registry(window);
    if (mywin != nullptr) { mywin->set_window_refresh(); }
}
void Window::setWindowFocus_callback(GLFWwindow *window, int focused) {
    Window *mywin = get_window_from_registry(window);
    if (mywin != nullptr) { mywin->set_window_focus(focused); }
}
void Window::setWindowIconify_callback(GLFWwindow *window, int iconified) {
    Window *mywin = get_window_from_registry(window);
    if (mywin != nullptr) { mywin->set_window_iconify(iconified); }
}
void Window::setWindowMaximize_callback(GLFWwindow *window, int maximized) {
    Window *mywin = get_window_from_registry(window);
    if (mywin != nullptr) { mywin->set_window_maximize(maximized); }
}
void Window::setFramebufferSize_callback(GLFWwindow *window, int width, int height) {
    Window *mywin = get_window_from_registry(window);
    if (mywin != nullptr) { mywin->set_framebuffer_size(width, height); }
}
void Window::setWindowContentScale_callback(GLFWwindow *window, float xscale, float yscale) {
    Window *mywin = get_window_from_registry(window);
    if (mywin != nullptr) { mywin->set_window_content_scale(xscale, yscale); }
}

void Window::set_window_pos(int, int) {}
void Window::set_window_size(int, int) {}
void Window::set_window_close() {}
void Window::set_window_refresh() {}
void Window::set_window_focus(int) {}
void Window::set_window_iconify(int) {}
void Window::set_window_maximize(int) {}
void Window::set_framebuffer_size(int, int) {}
void Window::set_window_content_scale(float, float) {}


void Window::pre_draw_hook() {
    glfwPollEvents();
}
void Window::post_draw_hook() {
}
void Window::main_loop() {
    while (!glfwWindowShouldClose(window())) {
        draw_frame();
    }
}


}// namespace balsa::visualization::glfw

#endif
