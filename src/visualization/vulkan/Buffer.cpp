
#include "balsa/visualization/vulkan/Buffer.hpp"
#include "balsa/visualization/vulkan/DeviceMemory.hpp"
#include "balsa/visualization/vulkan/Film.hpp"
#include "balsa/logging/stopwatch.hpp"
#include <vulkan/vulkan_structs.hpp>

namespace balsa::visualization::vulkan {

Buffer::Buffer(Buffer &&o) {
    m_device = o.m_device;
    m_handle = o.m_handle;
    m_memory = o.m_memory;

    o.m_handle = vk::Buffer{};
    o.free_memory();
}
Buffer &Buffer::operator=(Buffer &&o) {
    m_device = o.m_device;
    m_handle = o.m_handle;
    m_memory = o.m_memory;
    o.m_handle = vk::Buffer{};
    o.free_memory();
    return *this;
}

Buffer::Buffer(vk::Device device, vk::PhysicalDevice physical_device) : m_device(device), m_memory(new DeviceMemory(device, physical_device)) {
}

Buffer::Buffer(const Film &film) : Buffer(film.device(), film.physical_device()) {
}


Buffer::~Buffer() {
    destroy();
}
void Buffer::destroy() {
    auto sw = balsa::logging::hierarchical_stopwatch("Buffer::destroy");
    if (m_handle) {
        m_device.destroyBuffer(m_handle);
    }
    free_memory();
}

void Buffer::create(size_t data_size, vk::BufferUsageFlags usage) {
    assert(m_memory);
    assert(m_device);
    destroy();

    vk::BufferCreateInfo buffer_info{};
    buffer_info.setSize(data_size);
    buffer_info.setUsage(usage);
    buffer_info.setSharingMode(vk::SharingMode::eExclusive);
    m_handle = m_device.createBuffer(buffer_info);
}
void Buffer::create(size_t data_size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags properties) {
    assert(m_memory);
    create(data_size, usage);
    m_memory->create(memory_requirements(), data_size, properties);
    bind_memory(*m_memory);
}

vk::MemoryRequirements Buffer::memory_requirements() const {
    return m_device.getBufferMemoryRequirements(m_handle);
}

Buffer Buffer::create(const Film &film, size_t data_size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags properties) {

    spdlog::info("Creating buffer");
    Buffer buf(film);
    buf.create(data_size, usage, properties);

    return buf;
}

void Buffer::emplace_memory(const Film &film) {
    emplace_memory(film.physical_device());
}
void Buffer::emplace_memory(const vk::PhysicalDevice &physical_device) {

    m_memory = std::make_shared<DeviceMemory>(device(), physical_device);
}

void Buffer::bind_memory(DeviceMemory &memory) {
    m_device.bindBufferMemory(handle(), memory.handle(), m_offset);
}

void Buffer::free_memory() {
    vk::PhysicalDevice pd = m_memory->physical_device();
    m_memory.reset();
    m_memory = std::make_shared<DeviceMemory>(device(), pd);
}


}// namespace balsa::visualization::vulkan
