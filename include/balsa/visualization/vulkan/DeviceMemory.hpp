#if !defined(BALSA_VISUALIZATION_VULKAN_DeviceMemory_HPP)
#define BALSA_VISUALIZATION_VULKAN_DeviceMemory_HPP
#include <memory>
#include <vulkan/vulkan.hpp>
#include <span>

namespace balsa::visualization::vulkan {
class Film;
class Buffer;
class DeviceMemory {

  public:
    DeviceMemory(const Film &film);
    DeviceMemory(vk::Device device, vk::PhysicalDevice);
    DeviceMemory(DeviceMemory &&o);
    DeviceMemory &operator=(DeviceMemory &&o);

    ~DeviceMemory();


    vk::DeviceMemory handle() const { return m_handle; }
    bool has_handle() const;

    void create(vk::MemoryRequirements, size_t data_size, vk::MemoryPropertyFlags);

    void bind_buffer(Buffer &buffer);

    void free();


    void upload_data_noT(void *data, size_t data_size, vk::DeviceSize offset = 0);
    template<typename T, std::size_t Extent>
    void upload_data(const std::span<T, Extent> &values, vk::DeviceSize offset = 0);

    void destroy();

    vk::Device device() const { return m_device; }
    vk::PhysicalDevice physical_device() const { return m_physical_device; }


  private:
    vk::Device m_device;
    vk::PhysicalDevice m_physical_device;
    vk::DeviceMemory m_handle;
};

template<typename T, std::size_t Extent>
void DeviceMemory::upload_data(const std::span<T, Extent> &data, vk::DeviceSize offset) {
    upload_data_noT((void *)(data.data()), sizeof(T) * data.size(), offset);
}
}// namespace balsa::visualization::vulkan

#endif
