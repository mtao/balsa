#pragma once

#include "balsa/visualization/vulkan/buffer.hpp"
#include <vector>

namespace balsa::visualization::vulkan {

// Keeps staging buffers alive and submits their copies together before the
// frame's drawing commands. Buffers must not be used by the GPU before
// submit().
class BufferUploadBatch {
  public:
    explicit BufferUploadBatch(Film &film);
    ~BufferUploadBatch();
    BufferUploadBatch(const BufferUploadBatch &) = delete;
    auto operator=(const BufferUploadBatch &) -> BufferUploadBatch & = delete;

    void submit();

  private:
    friend class VulkanBuffer;
    void add(vk::Buffer destination, VulkanBuffer staging, vk::DeviceSize size);

    struct Copy {
        vk::Buffer destination;
        VulkanBuffer staging;
        vk::DeviceSize size;
    };
    Film &_film;
    std::vector<Copy> _copies;
};

} // namespace balsa::visualization::vulkan
