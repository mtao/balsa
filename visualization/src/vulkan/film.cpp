#include "balsa/visualization//vulkan/film.hpp"
#include <stdexcept>


namespace balsa::visualization::vulkan {
Film::~Film() {}

uint32_t Film::find_memory_type(uint32_t type_filter, vk::MemoryPropertyFlags properties) const {
    auto mem_props = physical_device().getMemoryProperties();
    for (uint32_t i = 0; i < mem_props.memoryTypeCount; ++i) {
        if ((type_filter & (1u << i)) && (mem_props.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    throw std::runtime_error("Film::find_memory_type: no suitable memory type found");
}
}// namespace balsa::visualization::vulkan
