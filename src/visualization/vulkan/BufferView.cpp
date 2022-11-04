#include "balsa/visualization/vulkan/Buffer.hpp"
#include "balsa/visualization/vulkan/BufferView.hpp"
#include "balsa/visualization/vulkan/Film.hpp"

namespace balsa::visualization::vulkan {
BufferView::BufferView() {}
BufferView::BufferView(std::shared_ptr<Buffer> buf, vk::DeviceSize offset) : m_buffer(std::move(buf)), m_offset(offset) {}
BufferView::~BufferView() {}

void BufferView::set_buffer(std::shared_ptr<Buffer> buf, vk::DeviceSize offset) {
    m_buffer = std::move(buf);
    m_offset = offset;
}
std::tuple<vk::Buffer, vk::DeviceSize> VertexBufferView::bind_info() const {
    auto ptr = buffer_ptr();
    if (ptr) {
        return std::make_tuple(ptr->handle(), offset());
    } else {
        return {};
    }
}
std::tuple<vk::Buffer, vk::DeviceSize, vk::IndexType> IndexBufferView::bind_info() const {
    auto ptr = buffer_ptr();
    if (ptr) {
        return std::make_tuple(ptr->handle(), offset(), m_index_type);
    } else {
        return {};
    }
}
void VertexBufferView::set_binding_description(vk::VertexInputBindingDescription binding_description) {
    m_binding_description = binding_description;
}
void VertexBufferView::set_attribute_descriptions(const std::span<vk::VertexInputAttributeDescription> &attribute_descriptions) {
    m_attribute_descriptions.assign(attribute_descriptions.begin(), attribute_descriptions.end());
}
vk::VertexInputBindingDescription VertexBufferView::binding_description() const {
    return m_binding_description;
}
std::span<const vk::VertexInputAttributeDescription> VertexBufferView::attribute_descriptions() const {
    return std::span{ m_attribute_descriptions.begin(), m_attribute_descriptions.end() };
}
size_t VertexBufferView::attribute_descriptions_size() const {
    return m_attribute_descriptions.size();
}
void IndexBufferView::set_index_type(vk::IndexType type) {
    m_index_type = type;
}
void IndexBufferView::bind(vk::CommandBuffer &command_buffer) const {

    auto [buf, off, type] = bind_info();
    command_buffer.bindIndexBuffer(buf, off, type);
}
void IndexBufferView::draw(vk::CommandBuffer &command_buffer) const {
    bind(command_buffer);
    command_buffer.drawIndexed(
      m_index_count, m_instance_count, m_first_index, m_vertex_offset, m_first_instance);
}

}// namespace balsa::visualization::vulkan
