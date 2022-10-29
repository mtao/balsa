
#include "balsa/visualization/vulkan/Buffer.hpp"
#include "balsa/visualization/vulkan/Film.hpp"
#include "balsa/visualization/vulkan/utils/BufferCopier.hpp"
#include <vulkan/vulkan.hpp>
#include <spdlog/spdlog.h>

namespace balsa::visualization::vulkan::utils {
BufferCopier BufferCopier::from_film(Film const &film, vk::Fence const &fence) {
    return BufferCopier(
      film.device(),
      film.graphics_command_pool(),
      film.graphics_queue(),
      fence);
}
BufferCopier::BufferCopier(vk::Device const &device, vk::CommandPool const &command_pool, vk::Queue const &queue, vk::Fence const &fence) : m_device(device), m_command_pool(command_pool), m_queue(queue), m_fence(fence) {}

void BufferCopier::operator()(const Buffer &src, Buffer &dst, vk::DeviceSize size, vk::DeviceSize src_offset, vk::DeviceSize dst_offset) const {

    assert(bool(m_device));
    assert(m_device == src.device());
    assert(m_device == dst.device());

    assert(bool(m_command_pool));
    assert(bool(m_queue));


    vk::CommandBufferAllocateInfo alloc_info{};
    alloc_info.setLevel(vk::CommandBufferLevel::ePrimary);
    alloc_info.setCommandPool(m_command_pool);
    alloc_info.setCommandBufferCount(1);

    vk::CommandBuffer command_buffer = m_device.allocateCommandBuffers(alloc_info)[0];
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

    m_device.freeCommandBuffers(m_command_pool, command_buffer);
}
// void BufferCopier::staged_from_device(void *data, vk::DeviceSize size, Buffer &dst, vk::DeviceSize dst_offset ) const
//{
//        balsa::visualization::vulkan::Buffer staging_buffer(film);

//        staging_buffer.create(size, vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
//        staging_buffer.upload_data_noT(data, size);

//        ->create(data_size, vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst, vk::MemoryPropertyFlagBits::eDeviceLocal);

//}

}// namespace balsa::visualization::vulkan::utils
