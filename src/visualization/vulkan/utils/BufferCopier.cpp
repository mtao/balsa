
#include "balsa/visualization/vulkan/Buffer.hpp"
#include "balsa/visualization/vulkan/DeviceMemory.hpp"
#include "balsa/visualization/vulkan/Film.hpp"
#include "balsa/visualization/vulkan/utils/BufferCopier.hpp"
#include <vulkan/vulkan.hpp>
#include <spdlog/spdlog.h>

namespace balsa::visualization::vulkan::utils {
BufferCopier BufferCopier::from_film(Film const &film, vk::Fence const &fence) {
    return BufferCopier(
      film.device(),
      film.physical_device(),
      film.graphics_command_pool(),
      film.graphics_queue(),
      fence);
}
BufferCopier::BufferCopier(vk::Device const &device, vk::PhysicalDevice const &physical_device, vk::CommandPool const &command_pool, vk::Queue const &queue, vk::Fence const &fence) : m_command_pool(command_pool), m_queue(queue), m_fence(fence), m_staging_buffer(device, physical_device) {
    m_staging_buffer.emplace_memory(physical_device);
}


void BufferCopier::operator()(const Buffer &src, Buffer &dst, vk::DeviceSize size, vk::DeviceSize src_offset, vk::DeviceSize dst_offset) const {

    assert(bool(device()));
    assert(device() == src.device());
    assert(device() == dst.device());

    assert(bool(m_command_pool));
    assert(bool(m_queue));


    vk::CommandBufferAllocateInfo alloc_info{};
    alloc_info.setLevel(vk::CommandBufferLevel::ePrimary);
    alloc_info.setCommandPool(m_command_pool);
    alloc_info.setCommandBufferCount(1);

    vk::CommandBuffer command_buffer = device().allocateCommandBuffers(alloc_info)[0];
    assert(bool(command_buffer));

    vk::CommandBufferBeginInfo begin_info;
    begin_info.setFlags(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
    command_buffer.begin(begin_info);

    vk::BufferCopy copy_region;
    copy_region.setSrcOffset(src_offset);
    copy_region.setDstOffset(dst_offset);
    copy_region.setSize(size);
    command_buffer.copyBuffer(src.handle(), dst.handle(), copy_region);

    command_buffer.end();

    vk::SubmitInfo submit_info{};
    submit_info.setCommandBuffers(command_buffer);


    if (m_fence) {
        spdlog::info("BufferCopier had fence");
        m_queue.submit(submit_info, m_fence);
    } else {
        spdlog::info("BufferCopier had no fence");
        m_queue.submit(submit_info);
        m_queue.waitIdle();
    }

    device().freeCommandBuffers(m_command_pool, command_buffer);
}


void BufferCopier::copy_direct(Buffer &buffer, void *data, vk::DeviceSize data_size, vk::BufferUsageFlags buffer_usage_flags, vk::MemoryPropertyFlags memory_property_flags) {

    disable_staging_buffer();
    assert(buffer.memory());
    buffer.create(data_size, buffer_usage_flags, memory_property_flags);
    buffer.memory()->upload_data_noT(data, data_size);
}

bool BufferCopier::staging_buffer_enabled() const {
    return m_staging_buffer_size.has_value();
}
void BufferCopier::enable_staging_buffer() {
    if (!m_staging_buffer_size.has_value()) {
        m_staging_buffer_size = 0;
    }
}
void BufferCopier::disable_staging_buffer() {
    m_staging_buffer_size = {};
    m_staging_buffer.free_memory();
}

void BufferCopier::create_staging_buffer(vk::DeviceSize data_size) {
    m_staging_buffer.create(data_size, vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
    m_staging_buffer_size = data_size;
}

void BufferCopier::copy_with_staging_buffer(Buffer &buffer, void *data, vk::DeviceSize data_size, vk::BufferUsageFlags buffer_usage_flags, vk::MemoryPropertyFlags memory_property_flags) {

    buffer.create(data_size, buffer_usage_flags | vk::BufferUsageFlagBits::eTransferDst, memory_property_flags);


    // staging buffer opt must exist already so this opt is directly readable
    if (*m_staging_buffer_size < data_size || VkBuffer(m_staging_buffer.handle()) == VK_NULL_HANDLE) {
        create_staging_buffer(data_size);
    }
    m_staging_buffer.memory()->upload_data_noT(data, data_size);

    operator()(m_staging_buffer, buffer, data_size);
}


void BufferCopier::copy(Buffer &buffer, void *data, vk::DeviceSize data_size, vk::BufferUsageFlags buffer_usage_flags, vk::MemoryPropertyFlags memory_property_flags) {
    if (staging_buffer_enabled()) {
        copy_with_staging_buffer(buffer, data, data_size, buffer_usage_flags, memory_property_flags);
    } else {
        copy_direct(buffer, data, data_size, buffer_usage_flags, memory_property_flags);
    }
}

}// namespace balsa::visualization::vulkan::utils
