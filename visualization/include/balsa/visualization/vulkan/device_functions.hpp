#if !defined(BALSA_VISUALIZATION_VULKAN_DEVIE_FUNCTIONS_HPP)
#define BALSA_VISUALIZATION_VULKAN_DEVIE_FUNCTIONS_HPP

#include <vulkan/vulkan_raii.hpp>
#include <set>
#include "film.hpp"

namespace balsa::visualization::vulkan {
struct DeviceFunctions {
    DeviceFunctions() = default;
    DeviceFunctions(const vk::Instance &instance);

    VKAPI_ATTR VkResult VKAPI_CALL vkCreateDebugUtilsMessengerEXT(
      VkInstance instance,
      const VkDebugUtilsMessengerCreateInfoEXT *pCreateInfo,
      const VkAllocationCallbacks *pAllocator,
      VkDebugUtilsMessengerEXT *pMessenger);

    VKAPI_ATTR void VKAPI_CALL vkDestroyDebugUtilsMessengerEXT(
      VkInstance instance,
      VkDebugUtilsMessengerEXT messenger,
      VkAllocationCallbacks const *pAllocator);

    VKAPI_ATTR VkResult VKAPI_CALL vkGetPhysicalDeviceSurfaceSuuportKHR(
      VkPhysicalDevice physicalDevice,
      uint32_t queueFamilyIndex,
      VkSurfaceKHR surface,
      VkBool32 *pSupported);

  private:
    PFN_vkCreateDebugUtilsMessengerEXT pfnVkCreateDebugUtilsMessengerEXT = nullptr;
    PFN_vkDestroyDebugUtilsMessengerEXT pfnVkDestroyDebugUtilsMessengerEXT =
      nullptr;
    PFN_vkGetPhysicalDeviceSurfaceSupportKHR
      pfnVkGetPhysicalDeviceSurfaceSupportKHR = nullptr;
};
}// namespace balsa::visualization::vulkan

#endif
