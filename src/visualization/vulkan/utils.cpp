#include "balsa/visualization/vulkan/utils.hpp"
//#include <range/v3/view/enumerate.hpp>
namespace balsa::visualization::vulkan {
uint32_t find_memory_type(Film const &film, uint32_t type_filter, vk::MemoryPropertyFlags flags) {
    auto mem_properties = film.physical_device().getMemoryProperties();
    return find_memory_type(mem_properties, type_filter, flags);
}
uint32_t find_memory_type(vk::PhysicalDeviceMemoryProperties const &mem_properties, uint32_t type_filter, vk::MemoryPropertyFlags flags) {
    for (uint32_t i = 0; i < mem_properties.memoryTypeCount; ++i) {
        if ((type_filter & (1 << i)) && (mem_properties.memoryTypes[i].propertyFlags & flags) == flags) {
            return i;
        }
    }
    throw std::runtime_error("failed to find suitable memory type!");
}
}// namespace balsa::visualization::vulkan
