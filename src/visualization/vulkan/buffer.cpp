
#include <balsa/visualization/vulkan/utils.hpp>
#include "balsa/visualization/vulkan/buffer.hpp"
#include "balsa/visualization/vulkan/film.hpp"

namespace balsa::visualization::vulkan {
Buffer::Buffer() {}
Buffer::Buffer(vk::Device device) : m_device(device) {}

Buffer::Buffer(const Film &film) : Buffer(film.device()) {}


void Buffer::set_device(vk::Device device) {
    m_device = device;
}
void Buffer::set_device(const Film &film) {
    set_device(film.device());
}

void Buffer::bind(const Film &film) {
    assert(m_device == film.device());
    auto cb = film.current_command_buffer();
    bind(cb);
}
void Buffer::bind(const vk::CommandBuffer &command_buffer) {
    command_buffer.bindVertexBuffers(0, m_buffer, size_t(0));
}

Buffer::~Buffer() {
    destroy();
}
void Buffer::destroy() {
    if (m_buffer) {
        m_device.destroyBuffer(m_buffer);
    }
    if (m_memory) {
        m_device.freeMemory(m_memory);
    }
}

void Buffer::create(const Film &film, size_t data_size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags properties) {
    assert(m_device == film.device());
    destroy();

    vk::BufferCreateInfo buffer_info{};
    buffer_info.setSize(data_size);
    buffer_info.setUsage(usage);
    buffer_info.setSharingMode(vk::SharingMode::eExclusive);
    m_buffer = m_device.createBuffer(buffer_info);

    vk::MemoryRequirements mem_requirements = m_device.getBufferMemoryRequirements(m_buffer);

    uint32_t memory_type_index = balsa::visualization::vulkan::find_memory_type(film, mem_requirements.memoryTypeBits, properties);

    vk::MemoryAllocateInfo alloc_info;
    alloc_info.setAllocationSize(mem_requirements.size);
    alloc_info.setMemoryTypeIndex(memory_type_index);

    m_memory = m_device.allocateMemory(alloc_info);

    m_device.bindBufferMemory(m_buffer, m_memory, 0);
}

void Buffer::upload_data_noT(const Film &film, void *data, size_t data_size) {
    assert(m_device == film.device());

    assert(m_buffer);
    assert(m_memory);


    void *map_data = m_device.mapMemory(m_memory, 0, data_size);
    memcpy(map_data, data, size_t(data_size));
    m_device.unmapMemory(m_memory);
}


}// namespace balsa::visualization::vulkan
