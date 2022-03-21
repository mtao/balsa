#if !defined(BALSA_VISUALIZATION_GLFW_VULKAN_FILM_HPP)
#define BALSA_VISUALIZATION_GLFW_VULKAN_FILM_HPP

#include "balsa/visualization/vulkan/native_film.hpp"
class GLFWwindow;


namespace balsa::visualization::glfw::vulkan {
class Film : public visualization::vulkan::NativeFilm {
  public:
    Film(GLFWwindow *window);
    ~Film();

  private:
    // Whatever window management tool we have is in charge of this
    vk::raii::SurfaceKHR make_surface() const override;
    vk::Extent2D choose_swapchain_extent() const override;
    std::vector<std::string> get_required_instance_extensions() const override;
    GLFWwindow *_window;
};
}// namespace balsa::visualization::glfw::vulkan

#endif
