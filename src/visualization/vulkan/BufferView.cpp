#include "balsa/visualization/vulkan/Buffer.hpp"
#include "balsa/visualization/vulkan/BufferView.hpp"
#include "balsa/visualization/vulkan/Film.hpp"

namespace balsa::visualization::vulkan {
BufferView::BufferView() {}
BufferView::BufferView(std::shared_ptr<Buffer> buf) : m_buffer(std::move(buf)) {}
BufferView::~BufferView() {}

void BufferView::set_buffer(std::shared_ptr<Buffer> buf) {
    m_buffer = std::move(buf);
}
std::tuple<vk::Buffer, vk::DeviceSize> BufferView::vertex_buffer_bind_info() {
    if (m_buffer) {
        return std::make_tuple(m_buffer->handle(), m_offset);
    } else {
        return {};
    }
}
}// namespace balsa::visualization::vulkan
