#include "balsa/visualization/vulkan/DeviceMemory.hpp"
#include "balsa/visualization/vulkan/utils/find_memory_type.hpp"
#include "balsa/visualization/vulkan/Film.hpp"
#include "balsa/logging/stopwatch.hpp"
#include <vulkan/vulkan_structs.hpp>

namespace balsa::visualization::vulkan {
DeviceMemory::DeviceMemory(DeviceMemory &&o) {
    m_device = o.m_device;
    m_handle = o.m_handle;
    m_physical_device = o.m_physical_device;

    o.m_handle = vk::DeviceMemory{};
}
DeviceMemory &DeviceMemory::operator=(DeviceMemory &&o) {
    m_device = o.m_device;
    m_physical_device = o.m_physical_device;
    m_handle = o.m_handle;
    o.m_handle = vk::DeviceMemory{};
    return *this;
}

DeviceMemory::DeviceMemory(const Film &film) : DeviceMemory(film.device(), film.physical_device()) {}
DeviceMemory::DeviceMemory(vk::Device device,
                           vk::PhysicalDevice physical_device) : m_device(device), m_physical_device(physical_device) {
}
void DeviceMemory::create(vk::MemoryRequirements mem_requirements, size_t data_size, vk::MemoryPropertyFlags properties) {
    uint32_t memory_type_index = balsa::visualization::vulkan::utils::find_memory_type(m_physical_device, mem_requirements.memoryTypeBits, properties);

    vk::MemoryAllocateInfo alloc_info;
    alloc_info.setAllocationSize(mem_requirements.size);
    alloc_info.setMemoryTypeIndex(memory_type_index);

    free();
    m_handle = m_device.allocateMemory(alloc_info);
}
bool DeviceMemory::has_handle() const {
    return m_handle;
}

DeviceMemory::~DeviceMemory() {
    destroy();
}
void DeviceMemory::destroy() {
    auto sw = balsa::logging::hierarchical_stopwatch("DeviceMemory::destroy");
    free();
}

void DeviceMemory::upload_data_noT(void *data, size_t data_size, vk::DeviceSize offset) {

    assert(m_handle);
    if (!bool(m_handle)) {
        spdlog::error("balsa::visualization::vulkan::DeviceMemory handle was not initialized");
    }


    void *map_data = m_device.mapMemory(m_handle, offset, data_size);
    memcpy(map_data, data, size_t(data_size));
    m_device.unmapMemory(m_handle);
}
void DeviceMemory::free() {
    if (m_handle) {
        m_device.freeMemory(m_handle);
    }
}
}// namespace balsa::visualization::vulkan

