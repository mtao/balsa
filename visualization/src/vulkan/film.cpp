#include "balsa/visualization//vulkan/film.hpp"
#include "balsa/visualization/vulkan/BufferAllocator.hpp"
#include <stdexcept>


namespace balsa::visualization::vulkan {
Film::~Film() { release_retired(); }

auto Film::buffer_allocator() -> std::shared_ptr<BufferAllocator> {
    if (!_buffer_allocator) {
        _buffer_allocator = std::make_shared<BufferAllocator>(
            instance(), physical_device(), device());
    }
    return _buffer_allocator;
}

void Film::release_buffer_allocator() { _buffer_allocator.reset(); }

void Film::release_retired() { _retired.clear(); }

void Film::collect_retired() {
    ++_frame_number;
    const auto frames = static_cast<uint64_t>(concurrent_frame_count());
    std::erase_if(_retired, [&](const Retired &retired) {
        return _frame_number - retired.frame >= frames;
    });
}

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
