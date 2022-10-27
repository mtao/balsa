#if !defined(BALSA_VISUALIZATION_VULKAN_BUFFER_VIEW_HPP)
#define BALSA_VISUALIZATION_VULKAN_BUFFER_VIEW_HPP
#include <memory>
#include <vulkan/vulkan.hpp>

namespace balsa::visualization::vulkan {
class Film;
class Buffer;
class PipelineFactory;
class BufferView {

  public:
    BufferView();
    BufferView(std::shared_ptr<Buffer>);
    void set_buffer(std::shared_ptr<Buffer>);

    std::tuple<vk::Buffer, vk::DeviceSize> vertex_buffer_bind_info();


    ~BufferView();

    bool has_buffer() const { return bool(m_buffer); }

    void destroy();

  private:
    std::shared_ptr<balsa::visualization::vulkan::Buffer> m_buffer;
    vk::DeviceSize m_offset;
};

}// namespace balsa::visualization::vulkan

#endif
