#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>

#include "balsa/visualization/vulkan/BufferAllocator.hpp"

#include <format>
#include <stdexcept>

namespace balsa::visualization::vulkan {

BufferAllocator::BufferAllocator(vk::Instance instance,
                                 vk::PhysicalDevice physical_device,
                                 vk::Device device) {
    VmaAllocatorCreateInfo info{};
    info.instance = instance;
    info.physicalDevice = physical_device;
    info.device = device;
    info.vulkanApiVersion = VK_API_VERSION_1_0;
    info.preferredLargeHeapBlockSize = 16 * 1024 * 1024;
    const auto result = vmaCreateAllocator(&info, &_allocator);
    if (result != VK_SUCCESS) {
        throw std::runtime_error(
            std::format("BufferAllocator: vmaCreateAllocator failed: {}",
                        vk::to_string(static_cast<vk::Result>(result))));
    }
}

BufferAllocator::~BufferAllocator() { vmaDestroyAllocator(_allocator); }

auto BufferAllocator::statistics() const -> Statistics {
    VmaTotalStatistics stats{};
    vmaCalculateStatistics(_allocator, &stats);
    return {stats.total.statistics.blockCount,
            stats.total.statistics.allocationCount,
            stats.total.statistics.blockBytes,
            stats.total.statistics.allocationBytes};
}

} // namespace balsa::visualization::vulkan
