#if !defined(BALSA_VISUALIZATION_VULKAN_UTILS_HPP)
#define BALSA_VISUALIZATION_VULKAN_UTILS_HPP
#include <glm/vec2.hpp>
#include <vulkan/vulkan.hpp>

namespace balsa::visualization::vulkan {
class Film;
namespace utils {
    uint32_t find_memory_type(Film const &film, uint32_t type_filter, vk::MemoryPropertyFlags flags);
    uint32_t find_memory_type(vk::PhysicalDevice const &physical_device, uint32_t type_filter, vk::MemoryPropertyFlags flags);
    uint32_t find_memory_type(vk::PhysicalDeviceMemoryProperties const &memoryProperties, uint32_t type_filter, vk::MemoryPropertyFlags flags);
}// namespace utils
}// namespace balsa::visualization::vulkan
#endif
