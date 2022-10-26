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

    void create(const vk::BufferCreateInfo &info);


    void bind(const Film &film);
    void bind(const vk::CommandBuffer &command_buffer);
    ~Buffer();

    template<typename T>
    void create(const Film &film, const T &values);

    void destroy();

  private:
    void create_noT(const Film &film, void *data, size_t data_size);
    vk::Device m_device;


    vk::Buffer m_buffer;
    vk::DeviceMemory m_memory;
};

template<typename DataType>
void Buffer::create(const Film &film, const DataType &data) {
    create_noT(film, (void *)(data.data()), sizeof(typename std::decay_t<DataType>::value_type) * data.size());
}
}// namespace balsa::visualization::vulkan

#endif
