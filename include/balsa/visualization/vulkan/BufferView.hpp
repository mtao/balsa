#if !defined(BALSA_VISUALIZATION_VULKAN_BUFFER_VIEW_HPP)
#define BALSA_VISUALIZATION_VULKAN_BUFFER_VIEW_HPP
#include <memory>
#include <vulkan/vulkan.hpp>
#include <span>

namespace balsa::visualization::vulkan {
class Film;
class Buffer;
class PipelineFactory;
class BufferView {

  public:
    BufferView();
    BufferView(std::shared_ptr<Buffer>, vk::DeviceSize offset = 0);
    void set_buffer(std::shared_ptr<Buffer>, vk::DeviceSize offset = 0);


    ~BufferView();

    bool has_buffer() const { return bool(m_buffer); }
    const std::shared_ptr<Buffer> &buffer_ptr() const { return m_buffer; }

    vk::DeviceSize offset() const {
        return m_offset;
    }

  private:
    std::shared_ptr<balsa::visualization::vulkan::Buffer> m_buffer;
    vk::DeviceSize m_offset = 0;
};

class VertexBufferView : public BufferView {
  public:
    std::tuple<vk::Buffer, vk::DeviceSize> bind_info() const;

    // NOTE: binding_description binding will be overwritten before use, does not need to be est
    vk::VertexInputBindingDescription binding_description() const;
    // NOTE: attribute descriptions binding will be overwritten before use, does not need to be set
    std::span<const vk::VertexInputAttributeDescription> attribute_descriptions() const;
    size_t attribute_descriptions_size() const;

    void set_binding_description(vk::VertexInputBindingDescription m_binding_description);
    void set_attribute_descriptions(const std::span<vk::VertexInputAttributeDescription> &attribute_descriptions);

  private:
    vk::VertexInputBindingDescription m_binding_description;
    std::vector<vk::VertexInputAttributeDescription> m_attribute_descriptions;
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
