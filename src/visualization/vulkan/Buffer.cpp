
#include "balsa/visualization/vulkan/utils/find_memory_type.hpp"
#include "balsa/visualization/vulkan/Buffer.hpp"
#include "balsa/visualization/vulkan/Film.hpp"
#include "balsa/logging/stopwatch.hpp"
#include <vulkan/vulkan_structs.hpp>

namespace balsa::visualization::vulkan {

Buffer::Buffer(Buffer &&o) {
    m_device = o.m_device;
    m_physical_device = o.m_physical_device;
    m_buffer = o.m_buffer;
    m_memory = o.m_memory;

    o.m_buffer = vk::Buffer{};
    o.m_memory = vk::DeviceMemory{};
}
Buffer &Buffer::operator=(Buffer &&o) {
    m_device = o.m_device;
    m_physical_device = o.m_physical_device;
    m_buffer = o.m_buffer;
    m_memory = o.m_memory;
    o.m_buffer = vk::Buffer{};
    o.m_memory = vk::DeviceMemory{};
    return *this;
}

Buffer::Buffer(vk::Device device,
               vk::PhysicalDevice physical_device) : m_device(device), m_physical_device(physical_device) {
}
Buffer::Buffer(const Film &film) {
    set_device(film);
}


void Buffer::set_device(const Film &film) {
    m_device = film.device();
    m_physical_device = film.physical_device();
}


Buffer::~Buffer() {
    destroy();
}
void Buffer::destroy() {
    auto sw = balsa::logging::hierarchical_stopwatch("Buffer::destroy");
    if (m_buffer) {
        m_device.destroyBuffer(m_buffer);
    }
    if (m_memory) {
        m_device.freeMemory(m_memory);
    }
}

void Buffer::create(size_t data_size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags properties) {
    auto sw = balsa::logging::hierarchical_stopwatch("Buffer::create");
    assert(m_device);
    destroy();

    vk::BufferCreateInfo buffer_info{};
    buffer_info.setSize(data_size);
    buffer_info.setUsage(usage);
    buffer_info.setSharingMode(vk::SharingMode::eExclusive);
    m_buffer = m_device.createBuffer(buffer_info);

    vk::MemoryRequirements mem_requirements = m_device.getBufferMemoryRequirements(m_buffer);

    uint32_t memory_type_index = balsa::visualization::vulkan::utils::find_memory_type(m_physical_device, mem_requirements.memoryTypeBits, properties);

    vk::MemoryAllocateInfo alloc_info;
    alloc_info.setAllocationSize(mem_requirements.size);
    alloc_info.setMemoryTypeIndex(memory_type_index);

    m_memory = m_device.allocateMemory(alloc_info);

    m_device.bindBufferMemory(m_buffer, m_memory, 0);
}


void Buffer::upload_data_noT(void *data, size_t data_size) {

    auto sw = balsa::logging::hierarchical_stopwatch("Buffer::upload_data");
    assert(m_buffer);
    assert(m_memory);


    void *map_data = m_device.mapMemory(m_memory, 0, data_size);
    memcpy(map_data, data, size_t(data_size));
    m_device.unmapMemory(m_memory);
}


}// namespace balsa::visualization::vulkan
