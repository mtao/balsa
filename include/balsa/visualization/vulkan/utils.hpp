#if !defined(BALSA_VISUALIZATION_VULKAN_UTILS_HPP)
#define BALSA_VISUALIZATION_VULKAN_UTILS_HPP
#include <balsa/visualization/vulkan/film.hpp>
#include <glm/vec2.hpp>

namespace balsa::visualization::vulkan {
uint32_t find_memory_type(Film const &film, uint32_t type_filter, vk::MemoryPropertyFlags flags);
uint32_t find_memory_type(vk::PhysicalDeviceMemoryProperties const &memoryProperties, uint32_t type_filter, vk::MemoryPropertyFlags flags);
}// namespace balsa::visualization::vulkan
#endif
