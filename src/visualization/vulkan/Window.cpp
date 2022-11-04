#include <spdlog/spdlog.h>
#include "balsa/visualization/vulkan/Window.hpp"
#include "balsa/visualization/vulkan/Film.hpp"

namespace balsa::visualization::vulkan {
Window::Window() {
}
Window::~Window() {
}
void Window::pre_draw_hook() {
    if (auto s = drawable(); s != nullptr) {
        s->begin_render_pass(film());
    }
}
void Window::post_draw_hook() {
    if (auto s = drawable(); s != nullptr) {
        s->end_render_pass(film());
    }
}

void Window::draw_frame() {
    pre_draw_hook();
    if (auto s = drawable(); s != nullptr) {
        s->draw(film());
    }
    post_draw_hook();
}

}// namespace balsa::visualization::vulkan
