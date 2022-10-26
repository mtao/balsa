#if !defined(BALSA_VISUALIZATION_VULKAN_BUFFER_HPP)
#define BALSA_VISUALIZATION_VULKAN_BUFFER_HPP
#include <memory>
#include <vulkan/vulkan.hpp>

namespace balsa::visualization::vulkan {
class Film;
class Buffer {

  public:
    Buffer();
    Buffer(vk::Device device);
    Buffer(const Film &film);
    void set_device(vk::Device device);
    void set_device(const Film &film);

    void upload_data(const vk::BufferCreateInfo &info);


    void bind(const Film &film);
    void bind(const vk::CommandBuffer &command_buffer);
    ~Buffer();


    void create(const Film &film, size_t data_size, vk::BufferUsageFlags, vk::MemoryPropertyFlags);

    template<typename T>
    void upload_data(const Film &film, const T &values);

    void destroy();

  private:
    void upload_data_noT(const Film &film, void *data, size_t data_size);
    vk::Device m_device;


    vk::Buffer m_buffer;
    vk::DeviceMemory m_memory;
};

template<typename DataType>
void Buffer::upload_data(const Film &film, const DataType &data) {
    upload_data_noT(film, (void *)(data.data()), sizeof(typename std::decay_t<DataType>::value_type) * data.size());
}
}// namespace balsa::visualization::vulkan

#endif
