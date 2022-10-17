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

vk::Extent2D Film::choose_swapchain_extent() const {
    auto capabilities = physical_device_raii().getSurfaceCapabilitiesKHR(surface());
    vk::Extent2D cur_extent = capabilities.currentExtent;
    if (cur_extent.width == std::numeric_limits<uint32_t>::max()) {
        int h, w;
        glfwGetFramebufferSize(_window, &w, &h);
        cur_extent.width =
          std::clamp<uint32_t>(w, capabilities.minImageExtent.width, capabilities.maxImageExtent.height);
        cur_extent.height =
          std::clamp<uint32_t>(h, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
    }
    return cur_extent;
}

std::vector<std::string> Film::get_required_instance_extensions() const {
    uint32_t glfwExtensionCount = 0;
    const char **glfwExtensions;
    glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    std::vector<std::string> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);
    auto oext = NativeFilm::get_required_instance_extensions();
    extensions.insert(extensions.end(), oext.begin(), oext.end());
    return extensions;
}
}// namespace balsa::visualization::glfw::vulkan
