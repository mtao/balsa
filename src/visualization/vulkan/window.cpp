#include <spdlog/spdlog.h>
#include "balsa/visualization/vulkan/window.hpp"
#include "balsa/visualization/vulkan/film.hpp"

namespace balsa::visualization::vulkan {
Window::Window() {
}
Window::~Window() {
}
void Window::pre_draw_hook() {
    if (m_scene) {
        m_scene->begin_render_pass(film());
    }
}
void Window::post_draw_hook() {
    if (m_scene) {
        m_scene->end_render_pass(film());
    }
}

void Window::set_scene(std::shared_ptr<SceneBase> scene) {
    m_scene = std::move(scene);
}
const std::shared_ptr<SceneBase> &Window::scene() const {
    return m_scene;
}
void Window::draw_frame() {
    pre_draw_hook();
    if (m_scene) {
        m_scene->draw(film());
    }
    post_draw_hook();
}

}// namespace balsa::visualization::vulkan
