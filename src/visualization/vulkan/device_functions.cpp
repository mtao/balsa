#include "balsa/visualization/vulkan/device_functions.hpp"
#include <spdlog/spdlog.h>
namespace balsa::visualization::vulkan {


VKAPI_ATTR VkResult VKAPI_CALL DeviceFunctions::vkCreateDebugUtilsMessengerEXT(
  VkInstance instance,
  const VkDebugUtilsMessengerCreateInfoEXT *pCreateInfo,
  const VkAllocationCallbacks *pAllocator,
  VkDebugUtilsMessengerEXT *pMessenger) {
    if (pfnVkCreateDebugUtilsMessengerEXT != nullptr) {
        return pfnVkCreateDebugUtilsMessengerEXT(instance, pCreateInfo, pAllocator, pMessenger);
    } else {
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
}

VKAPI_ATTR void VKAPI_CALL DeviceFunctions::vkDestroyDebugUtilsMessengerEXT(
  VkInstance instance,
  VkDebugUtilsMessengerEXT messenger,
  VkAllocationCallbacks const *pAllocator) {
    if (pfnVkDestroyDebugUtilsMessengerEXT != nullptr) {
        pfnVkDestroyDebugUtilsMessengerEXT(instance, messenger, pAllocator);
    }
}

VKAPI_ATTR VkResult VKAPI_CALL DeviceFunctions::vkGetPhysicalDeviceSurfaceSuuportKHR(
  VkPhysicalDevice physicalDevice,
  uint32_t queueFamilyIndex,
  VkSurfaceKHR surface,
  VkBool32 *pSupported) {
    if (pfnVkGetPhysicalDeviceSurfaceSupportKHR != nullptr) {
        return pfnVkGetPhysicalDeviceSurfaceSupportKHR(
          physicalDevice, queueFamilyIndex, surface, pSupported);
    } else {
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
}

DeviceFunctions::DeviceFunctions(const vk::Instance &instance) {

    pfnVkCreateDebugUtilsMessengerEXT =
      reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
        instance.getProcAddr("vkCreateDebugUtilsMessengerEXT"));
    if (pfnVkCreateDebugUtilsMessengerEXT == nullptr) {
        spdlog::error("Was unable to set up debug messenger from dispatcher");
        return;
    }
    pfnVkDestroyDebugUtilsMessengerEXT =
      reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
        instance.getProcAddr("vkDestroyDebugUtilsMessengerEXT"));
    if (pfnVkDestroyDebugUtilsMessengerEXT == nullptr) {
        spdlog::error("Was unable to set up debug messenger from dispatcher");
        return;
    }

    pfnVkGetPhysicalDeviceSurfaceSupportKHR =
      reinterpret_cast<PFN_vkGetPhysicalDeviceSurfaceSupportKHR>(
        instance.getProcAddr("vkGetPhysicalDeviceSurfaceSupportKHR"));
    if (pfnVkGetPhysicalDeviceSurfaceSupportKHR == nullptr) {
        spdlog::error("Was unable to set up debug messenger from dispatcher");
        return;
    }
}
}// namespace balsa::visualization::vulkan
