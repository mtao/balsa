#if !defined(BALSA_VISUALIZATION_GLFW_VULKAN_WINDOW_HPP)
#define BALSA_VISUALIZATION_GLFW_VULKAN_WINDOW_HPP

#include <GLFW/glfw3.h>
#include "balsa/visualization/glfw/window.hpp"
#include <spdlog/spdlog.h>
#include <mutex>
#include <map>

// TODO: I do my own management

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

bool Window::is_GLFWwindow_managed(GLFWwindow *window) {
    std::scoped_lock sl(active_window_registry_mutex);
    return active_window_registry.find(window) != active_window_registry.end();
}

Window::Window(const std::string_view &title, int width, int height) {

    // don't want to make glfw accidentally create an opengl context
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    // glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
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
    glfwSetWindowPosCallback(m_window, Window::windowPos_callback);
    glfwSetWindowSizeCallback(m_window, Window::windowSize_callback);
    glfwSetWindowCloseCallback(m_window, Window::windowClose_callback);
    glfwSetWindowRefreshCallback(m_window, Window::windowRefresh_callback);
    glfwSetWindowFocusCallback(m_window, Window::windowFocus_callback);
    glfwSetWindowIconifyCallback(m_window, Window::windowIconify_callback);
    glfwSetWindowMaximizeCallback(m_window, Window::windowMaximize_callback);
    glfwSetFramebufferSizeCallback(m_window, Window::framebufferSize_callback);
    glfwSetWindowContentScaleCallback(m_window, Window::windowContentScale_callback);

    glfwSetKeyCallback(m_window, Window::key_callback);
    glfwSetCharCallback(m_window, Window::character_callback);
    glfwSetCursorPosCallback(m_window, Window::cursor_position_callback);
    glfwSetMouseButtonCallback(m_window, Window::mouse_button_callback);
    glfwSetScrollCallback(m_window, Window::scroll_callback);
}

void Window::windowPos_callback(GLFWwindow *window, int xpos, int ypos) {
    Window *mywin = get_window_from_registry(window);
    if (mywin != nullptr) { mywin->window_pos(xpos, ypos); }
}
void Window::windowSize_callback(GLFWwindow *window, int width, int height) {
    Window *mywin = get_window_from_registry(window);
    if (mywin != nullptr) { mywin->window_size(width, height); }
}
void Window::windowClose_callback(GLFWwindow *window) {
    Window *mywin = get_window_from_registry(window);
    if (mywin != nullptr) { mywin->window_close(); }
}
void Window::windowRefresh_callback(GLFWwindow *window) {
    Window *mywin = get_window_from_registry(window);
    if (mywin != nullptr) { mywin->window_refresh(); }
}
void Window::windowFocus_callback(GLFWwindow *window, int focused) {
    Window *mywin = get_window_from_registry(window);
    if (mywin != nullptr) { mywin->window_focus(focused); }
}
void Window::windowIconify_callback(GLFWwindow *window, int iconified) {
    Window *mywin = get_window_from_registry(window);
    if (mywin != nullptr) { mywin->window_iconify(iconified); }
}
void Window::windowMaximize_callback(GLFWwindow *window, int maximized) {
    Window *mywin = get_window_from_registry(window);
    if (mywin != nullptr) { mywin->window_maximize(maximized); }
}
void Window::framebufferSize_callback(GLFWwindow *window, int width, int height) {
    Window *mywin = get_window_from_registry(window);
    if (mywin != nullptr) { mywin->framebuffer_size(width, height); }
}
void Window::windowContentScale_callback(GLFWwindow *window, float xscale, float yscale) {
    Window *mywin = get_window_from_registry(window);
    if (mywin != nullptr) { mywin->window_content_scale(xscale, yscale); }
}

void Window::key_callback(GLFWwindow *window, int key, int scancode, int action, int mods) {
    Window *mywin = get_window_from_registry(window);
    if (mywin != nullptr) { mywin->key(key, scancode, action, mods); }
}

void Window::character_callback(GLFWwindow *window, unsigned int codepoint) {
    Window *mywin = get_window_from_registry(window);
    if (mywin != nullptr) { mywin->character(codepoint); }
}
void Window::cursor_position_callback(GLFWwindow *window, double xpos, double ypos) {
    Window *mywin = get_window_from_registry(window);
    if (mywin != nullptr) { mywin->cursor_position(xpos, ypos); }
}


void Window::mouse_button_callback(GLFWwindow *window, int button, int action, int mods) {
    Window *mywin = get_window_from_registry(window);
    if (mywin != nullptr) { mywin->mouse_button(button, action, mods); }
}

void Window::scroll_callback(GLFWwindow *window, double xoffset, double yoffset) {
    Window *mywin = get_window_from_registry(window);
    if (mywin != nullptr) { mywin->scroll(xoffset, yoffset); }
}

void Window::window_pos(int, int) {}
void Window::window_size(int, int) {}
void Window::window_close() {}
void Window::window_refresh() {}
void Window::window_focus(int) {}
void Window::window_iconify(int) {}
void Window::window_maximize(int) {}
void Window::framebuffer_size(int, int) {}
void Window::window_content_scale(float, float) {}

void Window::key(int, int, int, int) {}
void Window::character(unsigned int) {}
void Window::cursor_position(double, double) {}
void Window::mouse_button(int, int, int) {}
void Window::scroll(double, double) {}


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

// glfwSetMonitorCallback(monitor_callback);
//
// void Window::monitor_callback(GLFWmonitor *monitor, int event) {
//     if (event == GLFW_CONNECTED) {
//         // The monitor was connected
//     } else if (event == GLFW_DISCONNECTED) {
//         // The monitor was disconnected
//     }
// }


}// namespace balsa::visualization::glfw

#endif
