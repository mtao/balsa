
#include "balsa/visualization/glfw/vulkan/BasicWindow.hpp"
#include "balsa/visualization/vulkan/Drawable.hpp"
namespace balsa::visualization::glfw::vulkan {
void BasicWindow::set_drawable(const std::shared_ptr<visualization::vulkan::Drawable> &drawable) {
    m_drawable = drawable;
}
void BasicWindow::key(int key, int scancode, int action, int mods) {

    m_key_callback(key, scancode, action, mods);
}
void BasicWindow::set_key_callback(key_callback_type callback) {
    m_key_callback = std::move(callback);
}
}// namespace balsa::visualization::glfw::vulkan
