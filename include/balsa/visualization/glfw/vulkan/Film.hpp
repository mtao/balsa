#if !defined(BALSA_VISUALIZATION_GLFW_VULKAN_FILM_HPP)
#define BALSA_VISUALIZATION_GLFW_VULKAN_FILM_HPP

#include "balsa/visualization/vulkan/NativeFilm.hpp"
class GLFWwindow;


namespace balsa::visualization::glfw::vulkan {
class Film : public visualization::vulkan::NativeFilm {
  public:
    Film(GLFWwindow *window, const std::vector<std::string> &device_extensions = {}, const std::vector<std::string> &instance_extensions = {}, const std::vector<std::string> &validation_layers = {});
    Film(Film &&) = default;
    Film &operator=(Film &&) = default;
    ~Film();

    void set_window(GLFWwindow *window);

  private:
    // Whatever window management tool we have is in charge of this
    vk::raii::SurfaceKHR make_surface() const override;
    vk::Extent2D choose_swapchain_extent() const override;
    std::vector<std::string> get_required_instance_extensions() const override;
    GLFWwindow *m_window;
};
}// namespace balsa::visualization::glfw::vulkan

#endif
