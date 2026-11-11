
#include "balsa/visualization/vulkan/BufferAllocator.hpp"
#include "balsa/visualization/vulkan/BufferUploadBatch.hpp"
#include "balsa/visualization/vulkan/buffer.hpp"
#include "balsa/visualization/vulkan/film.hpp"
#include "balsa/visualization/vulkan/one_shot_command.hpp"
#include <cstring>
#include <format>
#include <stdexcept>
#include <utility>
#include <vk_mem_alloc.h>

namespace balsa::visualization::vulkan {

VulkanBuffer::VulkanBuffer(Film &film,
                           vk::DeviceSize size,
                           vk::BufferUsageFlags usage,
                           vk::MemoryPropertyFlags mem_props)
  : VulkanBuffer(film.buffer_allocator(), size, usage, mem_props) {}

VulkanBuffer::VulkanBuffer(std::shared_ptr<BufferAllocator> allocator,
                           vk::DeviceSize size,
                           vk::BufferUsageFlags usage,
                           vk::MemoryPropertyFlags mem_props)
  : _allocator(std::move(allocator)), _size(size),
    _host_visible(bool(mem_props & vk::MemoryPropertyFlagBits::eHostVisible)) {
    if (!_allocator || size == 0) {
        throw std::invalid_argument(
            "VulkanBuffer: an allocator and nonzero size are required");
    }
    const VkBufferCreateInfo buffer_info =
        vk::BufferCreateInfo{}.setSize(size).setUsage(usage).setSharingMode(
            vk::SharingMode::eExclusive);
    VmaAllocationCreateInfo allocation_info{};
    allocation_info.requiredFlags =
        static_cast<VkMemoryPropertyFlags>(mem_props);
    if (_host_visible) allocation_info.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;
    VkBuffer buffer = VK_NULL_HANDLE;
    VmaAllocationInfo allocated{};
    const auto result = vmaCreateBuffer(_allocator->_allocator,
                                        &buffer_info,
                                        &allocation_info,
                                        &buffer,
                                        &_allocation,
                                        &allocated);
    if (result != VK_SUCCESS) {
        throw std::runtime_error(
            std::format("VulkanBuffer: vmaCreateBuffer failed ({} bytes, usage "
                        "{}, memory {}): {}",
                        size,
                        vk::to_string(usage),
                        vk::to_string(mem_props),
                        vk::to_string(static_cast<vk::Result>(result))));
    }
    _buffer = buffer;
    _mapped = allocated.pMappedData;
}

VulkanBuffer::~VulkanBuffer() {
    release();
}

VulkanBuffer::VulkanBuffer(VulkanBuffer &&other) noexcept
  : _buffer(std::exchange(other._buffer, {})),
    _allocator(std::move(other._allocator)),
    _allocation(std::exchange(other._allocation, nullptr)),
    _mapped(std::exchange(other._mapped, nullptr)),
    _size(std::exchange(other._size, 0)),
    _host_visible(std::exchange(other._host_visible, false)) {}

VulkanBuffer &VulkanBuffer::operator=(VulkanBuffer &&other) noexcept {
    if (this != &other) {
        release();
        _buffer = std::exchange(other._buffer, {});
        _allocator = std::move(other._allocator);
        _allocation = std::exchange(other._allocation, nullptr);
        _mapped = std::exchange(other._mapped, nullptr);
        _size = std::exchange(other._size, 0);
        _host_visible = std::exchange(other._host_visible, false);
    }
    return *this;
}

void VulkanBuffer::release() {
    if (_allocator && _buffer) {
        vmaDestroyBuffer(_allocator->_allocator, _buffer, _allocation);
    }
    _buffer = vk::Buffer{};
    _allocation = nullptr;
    _mapped = nullptr;
    _allocator.reset();
    _size = 0;
    _host_visible = false;
}

void VulkanBuffer::upload(const void *data, vk::DeviceSize size, vk::DeviceSize offset) {
    if (!_host_visible) {
        throw std::runtime_error(
          "VulkanBuffer::upload: buffer is not host-visible; use upload_staged() instead");
    }
    if (offset > _size || size > _size - offset) {
        throw std::runtime_error(
          "VulkanBuffer::upload: write exceeds buffer size");
    }

    if (size == 0) return;
    std::memcpy(static_cast<std::byte *>(_mapped) + offset,
                data,
                static_cast<size_t>(size));
    const auto result =
        vmaFlushAllocation(_allocator->_allocator, _allocation, offset, size);
    if (result != VK_SUCCESS) {
        throw std::runtime_error(
            std::format("VulkanBuffer: vmaFlushAllocation failed: {}",
                        vk::to_string(static_cast<vk::Result>(result))));
    }
}

void VulkanBuffer::upload_staged(Film &film, const void *data, vk::DeviceSize size) {
    if (size > _size) {
        throw std::runtime_error(
          "VulkanBuffer::upload_staged: data exceeds buffer size");
    }

    if (size == 0) return;
    VulkanBuffer staging(_allocator,
                         size,
                         vk::BufferUsageFlagBits::eTransferSrc,
                         vk::MemoryPropertyFlagBits::eHostVisible
                             | vk::MemoryPropertyFlagBits::eHostCoherent);
    staging.upload(data, size);
    if (auto *batch = film.buffer_upload_batch()) {
        batch->add(_buffer, std::move(staging), size);
        return;
    }

    one_shot_command(film, [&](vk::CommandBuffer cmd) {
        vk::BufferCopy copy_region{};
        copy_region.size = size;
        cmd.copyBuffer(staging.buffer(), _buffer, copy_region);
    });

}

}// namespace balsa::visualization::vulkan
