#if !defined(BALSA_VISUALIZATION_VULKAN_BUFFER_HPP)
#define BALSA_VISUALIZATION_VULKAN_BUFFER_HPP
#include <memory>
#include <vulkan/vulkan.hpp>

namespace balsa::visualization::vulkan {
class Film;
class DeviceMemory;
class Buffer {

  public:
    static Buffer create(const Film &film, size_t data_size, vk::BufferUsageFlags, vk::MemoryPropertyFlags);
    Buffer(const Film &film);
    Buffer(vk::Device device, vk::PhysicalDevice physical_device);
    Buffer(Buffer &&o);
    Buffer &operator=(Buffer &&o);


    ~Buffer();


    const vk::Buffer &handle() const { return m_handle; }
    const std::shared_ptr<DeviceMemory> &memory() const { return m_memory; }


    void create(size_t data_size, vk::BufferUsageFlags);
    void create(size_t data_size, vk::BufferUsageFlags, vk::MemoryPropertyFlags);

    // create memory inside
    void emplace_memory(const Film &film);
    void emplace_memory(const vk::PhysicalDevice &physical_device);


    void free_memory();


    void destroy();

    vk::Device device() const { return m_device; }

  private:
    vk::Device m_device;


    vk::Buffer m_handle;
    // TODO: write a memory view
    std::shared_ptr<DeviceMemory> m_memory;
    size_t m_offset = 0;

    vk::MemoryRequirements memory_requirements() const;
    void bind_memory(DeviceMemory &memory);
};
}// namespace balsa::visualization::vulkan

#endif
