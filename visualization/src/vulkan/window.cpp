#include "balsa/visualization/vulkan/window.hpp"
#include "balsa/visualization/vulkan/film.hpp"
#include "balsa/visualization/input_handler.hpp"

namespace balsa::visualization::vulkan {

Window::Window() = default;
Window::~Window() = default;

void Window::set_scene(std::shared_ptr<SceneBase> scene) {
    _scene = std::move(scene);
}

const std::shared_ptr<SceneBase> &Window::scene() const {
    return _scene;
}

void Window::set_input_handler(std::shared_ptr<InputHandler> handler) {
    _input_handler = std::move(handler);
}

const std::shared_ptr<InputHandler> &Window::input_handler() const {
    return _input_handler;
}

void Window::pre_draw_hook() {
    if (_scene) {
        _scene->begin_render_pass(film());
    }
}

void Window::post_draw_hook() {
    if (_scene) {
        _scene->end_render_pass(film());
    }
}

void Window::draw_frame() {
    pre_draw_hook();
    if (_scene) {
        _scene->draw(film());
    }
    post_draw_hook();
}

void Window::dispatch_mouse(const MouseEvent &e) {
    if (_input_handler) {
        _input_handler->on_mouse_event(e);
    }
}

void Window::dispatch_key(const KeyEvent &e) {
    if (_input_handler) {
        _input_handler->on_key_event(e);
    }
}

void Window::dispatch_resize(const WindowResizeEvent &e) {
    if (_input_handler) {
        _input_handler->on_window_resize(e);
    }
}

}// namespace balsa::visualization::vulkan
