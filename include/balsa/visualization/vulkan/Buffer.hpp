#if !defined(BALSA_VISUALIZATION_VULKAN_BUFFER_HPP)
#define BALSA_VISUALIZATION_VULKAN_BUFFER_HPP
#include <memory>
#include <vulkan/vulkan.hpp>

namespace balsa::visualization::vulkan {
class Film;
class Buffer {

  public:
    Buffer(const Film &film);
    Buffer(Buffer &&o);
    Buffer &operator=(Buffer &&o);
    void set_device(const Film &film);

    void upload_data(const vk::BufferCreateInfo &info);
    ~Buffer();

    vk::Buffer handle() const { return m_buffer; }

    void create(size_t data_size, vk::BufferUsageFlags, vk::MemoryPropertyFlags);


    template<typename T>
    void upload_data(const T &values);

    void destroy();

    vk::Device device() const { return m_device; }

  private:
    void upload_data_noT(void *data, size_t data_size);
    vk::Device m_device;
    vk::PhysicalDevice m_physical_device;


    vk::Buffer m_buffer;
    vk::DeviceMemory m_memory;
};

template<typename DataType>
void Buffer::upload_data(const DataType &data) {
    upload_data_noT((void *)(data.data()), sizeof(typename std::decay_t<DataType>::value_type) * data.size());
}
}// namespace balsa::visualization::vulkan

#endif
