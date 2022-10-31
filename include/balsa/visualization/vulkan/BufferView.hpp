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


    ~BufferView();

    bool has_buffer() const { return bool(m_buffer); }

    void destroy();
    vk::DeviceSize offset() const {
        return m_offset;
    }

  private:
    std::shared_ptr<balsa::visualization::vulkan::Buffer> m_buffer;
    vk::DeviceSize m_offset;
};
class VertexBufferView : public BufferView {
    std::tuple<vk::Buffer, vk::DeviceSize> bind_info() const;
};

class IndexBufferView : public BufferView {
  public:
    std::tuple<vk::Buffer, vk::DeviceSize, vk::IndexType> bind_info() const;


    template<typename IndexType>
    void set_index_type();
    void set_index_type(vk::IndexType);

  private:
    vk::IndexType m_index_type;
};
template<typename IndexType>
void IndexBufferView::set_index_type() {
    set_index_type(vk::IndexTypeValue<std::decay_t<IndexType>>::value);
}

}// namespace balsa::visualization::vulkan

#endif
