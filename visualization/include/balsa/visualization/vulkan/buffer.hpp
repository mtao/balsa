#pragma once
#include <cstring>
#include <memory>
#include <span>
#include <vulkan/vulkan.hpp>

struct VmaAllocation_T;

namespace balsa::visualization::vulkan {

class Film;
class BufferAllocator;

// RAII wrapper for a VkBuffer and its suballocation in shared device memory.
//
// Supports both host-visible (mappable) and device-local (staging-required)
// memory.  Use upload() for host-visible buffers and upload_staged() when
// the buffer lives in device-local memory.
//
// Non-copyable, movable.  Destroying or moving-from releases the Vulkan
// resources.
class VulkanBuffer {
  public:
    VulkanBuffer() = default;

    // Construct and allocate a buffer.
    //   film        -- provides the device's shared buffer allocator
    //   size        -- buffer size in bytes
    //   usage       -- VkBufferUsageFlags (e.g. eVertexBuffer | eTransferDst)
    //   mem_props   -- desired memory property flags
    VulkanBuffer(Film &film,
                 vk::DeviceSize size,
                 vk::BufferUsageFlags usage,
                 vk::MemoryPropertyFlags mem_props);

    ~VulkanBuffer();

    // Non-copyable
    VulkanBuffer(const VulkanBuffer &) = delete;
    VulkanBuffer &operator=(const VulkanBuffer &) = delete;

    // Movable
    VulkanBuffer(VulkanBuffer &&other) noexcept;
    VulkanBuffer &operator=(VulkanBuffer &&other) noexcept;

    // Upload data to a persistently mapped host-visible buffer.
    // The buffer must have been created with eHostVisible memory.
    void upload(const void *data, vk::DeviceSize size, vk::DeviceSize offset = 0);

    // Upload data through a staging buffer.
    // Creates a temporary host-visible buffer, copies data into it, then
    // records a vkCmdCopyBuffer and submits+waits on the graphics queue,
    // or defers the copy to the Film's active BufferUploadBatch.
    // Use this for device-local buffers. In a batch, do not use or release
    // the destination until the batch has been submitted.
    void upload_staged(Film &film, const void *data, vk::DeviceSize size);

    // Typed convenience: upload a span of trivially-copyable values.
    template<typename T>
    void upload(std::span<const T> data, vk::DeviceSize offset = 0) {
        upload(data.data(), data.size_bytes(), offset);
    }

    template<typename T>
    void upload_staged(Film &film, std::span<const T> data) {
        upload_staged(film, data.data(), data.size_bytes());
    }

    // Explicitly release Vulkan resources.  Safe to call multiple times.
    void release();

    vk::Buffer buffer() const { return _buffer; }
    vk::DeviceSize size() const { return _size; }
    bool is_valid() const { return _buffer != vk::Buffer{}; }
    explicit operator bool() const { return is_valid(); }

  private:
    VulkanBuffer(std::shared_ptr<BufferAllocator> allocator,
                 vk::DeviceSize size,
                 vk::BufferUsageFlags usage,
                 vk::MemoryPropertyFlags mem_props);

    vk::Buffer _buffer;
    std::shared_ptr<BufferAllocator> _allocator;
    VmaAllocation_T *_allocation = nullptr;
    void *_mapped = nullptr;
    vk::DeviceSize _size = 0;
    bool _host_visible = false;
};

}// namespace balsa::visualization::vulkan
