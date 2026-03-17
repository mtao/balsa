#if !defined(BALSA_VISUALIZATION_VULKAN_BUFFER_HPP)
#define BALSA_VISUALIZATION_VULKAN_BUFFER_HPP

#include <vulkan/vulkan.hpp>
#include <span>
#include <cstring>

namespace balsa::visualization::vulkan {

class Film;

// RAII wrapper for a VkBuffer + VkDeviceMemory pair.
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
    //   film        -- provides device + physical_device + find_memory_type
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

    // Upload data to a host-visible buffer via map/memcpy/unmap.
    // The buffer must have been created with eHostVisible memory.
    void upload(const void *data, vk::DeviceSize size, vk::DeviceSize offset = 0);

    // Upload data through a staging buffer.
    // Creates a temporary host-visible buffer, copies data into it, then
    // records a vkCmdCopyBuffer and submits+waits on the graphics queue.
    // Use this for device-local buffers.
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
    vk::Device _device;
    vk::Buffer _buffer;
    vk::DeviceMemory _memory;
    vk::DeviceSize _size = 0;
    bool _host_visible = false;
};

}// namespace balsa::visualization::vulkan

#endif
