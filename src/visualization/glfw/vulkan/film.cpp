#include "balsa/visualization/glfw/vulkan/film.hpp"
#include <spdlog/spdlog.h>
#include <GLFW/glfw3.h>


namespace balsa::visualization::glfw::vulkan {

Film::Film(GLFWwindow *window) : NativeFilm(nullptr), _window(window) {
    if (_window != nullptr) {
        initialize();
    }
}
Film::~Film() = default;
vk::raii::SurfaceKHR Film::make_surface() const {


    VkSurfaceKHR vksurface;
    vk::Result result = vk::Result(glfwCreateWindowSurface(
      static_cast<VkInstance>(instance()), _window, nullptr, &vksurface));
    if (result != vk::Result::eSuccess) {
        spdlog::error("Surface creation failed {}", to_string(result));
    }
    return vk::raii::SurfaceKHR(instance_raii(), vksurface);
}
}// namespace balsa::visualization::glfw::vulkan
