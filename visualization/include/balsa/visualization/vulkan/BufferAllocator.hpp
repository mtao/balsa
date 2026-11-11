#pragma once

#include <cstdint>
#include <vulkan/vulkan.hpp>

struct VmaAllocator_T;

namespace balsa::visualization::vulkan {

// Suballocates buffer memory from shared blocks on one Vulkan device.
// The device must outlive the allocator and every buffer that uses it.
class BufferAllocator {
  public:
    BufferAllocator(vk::Instance instance,
                    vk::PhysicalDevice physical_device,
                    vk::Device device);
    ~BufferAllocator();
    BufferAllocator(const BufferAllocator &) = delete;
    auto operator=(const BufferAllocator &) -> BufferAllocator & = delete;

    struct Statistics {
        uint32_t memory_blocks;
        uint32_t allocations;
        vk::DeviceSize reserved_bytes;
        vk::DeviceSize used_bytes;
    };
    auto statistics() const -> Statistics;

  private:
    friend class VulkanBuffer;
    VmaAllocator_T *_allocator = nullptr;
};

} // namespace balsa::visualization::vulkan
