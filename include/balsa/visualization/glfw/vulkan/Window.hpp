#if !defined(BALSA_VISUALIZATION_GLFW_VULKAN_WINDOW_HPP)
#define BALSA_VISUALIZATION_GLFW_VULKAN_WINDOW_HPP

#include <vector>
#include <vulkan/vulkan_core.h>
#include <vulkan/vulkan.hpp>
#include <glm/vec2.hpp>
#include "balsa/visualization/vulkan/Window.hpp"
#include "balsa/visualization/glfw/Window.hpp"
#include "balsa/visualization/glfw/vulkan/Film.hpp"


namespace balsa::visualization::glfw::vulkan {

class Window : public glfw::Window
  , public visualization::vulkan::Window {
  public:
    using GLFWParentType = glfw::Window;
    using VulkanParentType = visualization::vulkan::Window;
    Window(const std::string_view &title, int width = 600, int height = 400);

    int exec() override;

    void pre_draw_hook() override;
    void post_draw_hook() override;

    void resize(int w, int h) override;
    visualization::vulkan::Film &film() override;

    void draw_frame() override;

    void window_size(int w, int h) override;

  private:
    Film m_film;
};
}// namespace balsa::visualization::glfw::vulkan

#endif
