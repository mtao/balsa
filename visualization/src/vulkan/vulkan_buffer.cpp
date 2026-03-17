
#include "balsa/visualization/vulkan/buffer.hpp"
#include "balsa/visualization/vulkan/film.hpp"
#include <stdexcept>
#include <cstring>

namespace balsa::visualization::vulkan {

VulkanBuffer::VulkanBuffer(Film &film,
                           vk::DeviceSize size,
                           vk::BufferUsageFlags usage,
                           vk::MemoryPropertyFlags mem_props)
  : _device(film.device()), _size(size) {
    // Create the buffer object
    vk::BufferCreateInfo buf_info{};
    buf_info.size = size;
    buf_info.usage = usage;
    buf_info.sharingMode = vk::SharingMode::eExclusive;

    _buffer = _device.createBuffer(buf_info);

    // Query memory requirements and allocate
    auto mem_reqs = _device.getBufferMemoryRequirements(_buffer);

    vk::MemoryAllocateInfo alloc_info{};
    alloc_info.allocationSize = mem_reqs.size;
    alloc_info.memoryTypeIndex = film.find_memory_type(mem_reqs.memoryTypeBits, mem_props);

    _memory = _device.allocateMemory(alloc_info);
    _device.bindBufferMemory(_buffer, _memory, 0);

    _host_visible = (mem_props & vk::MemoryPropertyFlagBits::eHostVisible) == vk::MemoryPropertyFlagBits::eHostVisible;
}

VulkanBuffer::~VulkanBuffer() {
    release();
}

VulkanBuffer::VulkanBuffer(VulkanBuffer &&other) noexcept
  : _device(other._device),
    _buffer(other._buffer),
    _memory(other._memory),
    _size(other._size),
    _host_visible(other._host_visible) {
    other._device = vk::Device{};
    other._buffer = vk::Buffer{};
    other._memory = vk::DeviceMemory{};
    other._size = 0;
    other._host_visible = false;
}

VulkanBuffer &VulkanBuffer::operator=(VulkanBuffer &&other) noexcept {
    if (this != &other) {
        release();
        _device = other._device;
        _buffer = other._buffer;
        _memory = other._memory;
        _size = other._size;
        _host_visible = other._host_visible;
        other._device = vk::Device{};
        other._buffer = vk::Buffer{};
        other._memory = vk::DeviceMemory{};
        other._size = 0;
        other._host_visible = false;
    }
    return *this;
}

void VulkanBuffer::release() {
    if (_device && _buffer) {
        _device.destroyBuffer(_buffer);
        _buffer = vk::Buffer{};
    }
    if (_device && _memory) {
        _device.freeMemory(_memory);
        _memory = vk::DeviceMemory{};
    }
}

void VulkanBuffer::upload(const void *data, vk::DeviceSize size, vk::DeviceSize offset) {
    if (!_host_visible) {
        throw std::runtime_error(
          "VulkanBuffer::upload: buffer is not host-visible; use upload_staged() instead");
    }
    if (offset + size > _size) {
        throw std::runtime_error(
          "VulkanBuffer::upload: write exceeds buffer size");
    }

    void *mapped = _device.mapMemory(_memory, offset, size);
    std::memcpy(mapped, data, static_cast<size_t>(size));
    _device.unmapMemory(_memory);
}

void VulkanBuffer::upload_staged(Film &film, const void *data, vk::DeviceSize size) {
    if (size > _size) {
        throw std::runtime_error(
          "VulkanBuffer::upload_staged: data exceeds buffer size");
    }

    // 1. Create a temporary host-visible staging buffer
    vk::BufferCreateInfo staging_buf_info{};
    staging_buf_info.size = size;
    staging_buf_info.usage = vk::BufferUsageFlagBits::eTransferSrc;
    staging_buf_info.sharingMode = vk::SharingMode::eExclusive;

    vk::Buffer staging_buffer = _device.createBuffer(staging_buf_info);

    auto staging_reqs = _device.getBufferMemoryRequirements(staging_buffer);

    vk::MemoryAllocateInfo staging_alloc{};
    staging_alloc.allocationSize = staging_reqs.size;
    staging_alloc.memoryTypeIndex = film.find_memory_type(
      staging_reqs.memoryTypeBits,
      vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

    vk::DeviceMemory staging_memory = _device.allocateMemory(staging_alloc);
    _device.bindBufferMemory(staging_buffer, staging_memory, 0);

    // 2. Copy data into the staging buffer
    void *mapped = _device.mapMemory(staging_memory, 0, size);
    std::memcpy(mapped, data, static_cast<size_t>(size));
    _device.unmapMemory(staging_memory);

    // 3. Record and submit a copy command
    vk::CommandBufferAllocateInfo cmd_alloc{};
    cmd_alloc.commandPool = film.graphics_command_pool();
    cmd_alloc.level = vk::CommandBufferLevel::ePrimary;
    cmd_alloc.commandBufferCount = 1;

    auto cmd_buffers = _device.allocateCommandBuffers(cmd_alloc);
    vk::CommandBuffer cmd = cmd_buffers[0];

    vk::CommandBufferBeginInfo begin_info{};
    begin_info.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;
    cmd.begin(begin_info);

    vk::BufferCopy copy_region{};
    copy_region.size = size;
    cmd.copyBuffer(staging_buffer, _buffer, copy_region);

    cmd.end();

    // 4. Submit and wait
    vk::SubmitInfo submit_info{};
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &cmd;

    vk::Queue queue = film.graphics_queue();
    queue.submit(submit_info);
    queue.waitIdle();

    // 5. Clean up staging resources
    _device.freeCommandBuffers(film.graphics_command_pool(), cmd);
    _device.destroyBuffer(staging_buffer);
    _device.freeMemory(staging_memory);
}

}// namespace balsa::visualization::vulkan
