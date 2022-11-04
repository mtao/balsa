
#include "balsa/visualization/vulkan/utils/VertexBufferViewCollection.hpp"
#include "balsa/visualization/vulkan/utils/PipelineFactory.hpp"
#include <range/v3/view/transform.hpp>
#include <range/v3/numeric/accumulate.hpp>
#include <range/v3/view/enumerate.hpp>
#include <range/v3/view/join.hpp>
#include <spdlog/spdlog.h>

namespace balsa::visualization::vulkan::utils {

void VertexBufferViewCollection::clear() {
    m_views.clear();
}
void VertexBufferViewCollection::resize(size_t size) {
    m_views.resize(size);
}
void VertexBufferViewCollection::add_view(const VertexBufferView &view) {
    m_views.emplace_back(view);
}


std::vector<vk::VertexInputBindingDescription> VertexBufferViewCollection::binding_descriptions() const {
    auto indexed_views_nooff = m_views | ranges::views::enumerate;
    auto indexed_views = indexed_views_nooff | ranges::views::transform([&](auto pr) {
                             auto [binding_index, view] = pr;
                             binding_index += m_first_binding;
                             return pr;
                         });
    auto get_binding = [](auto &&pr) {
        const auto &[binding_index, view] = pr;

        auto bd = view.binding_description();
        bd.setBinding(binding_index);
        return bd;
    };

    return indexed_views | ranges::views::transform(get_binding) | ranges::to<std::vector>();
}
std::vector<vk::VertexInputAttributeDescription> VertexBufferViewCollection::attribute_descriptions() const {
    auto indexed_views_nooff = m_views | ranges::views::enumerate;
    auto indexed_views = indexed_views_nooff | ranges::views::transform([&](auto pr) {
                             auto [binding_index, view] = pr;
                             binding_index += m_first_binding;
                             return pr;
                         });

    auto get_attributes = [](auto &&pr) {
        const auto &[binding_index, view] = pr;
        auto sattr = view.attribute_descriptions();
        std::vector<vk::VertexInputAttributeDescription>
          attrs(sattr.begin(), sattr.end());
        for (auto &attr : attrs) {
            attr.setBinding(binding_index);
        }
        return attrs;
    };


    return indexed_views | ranges::views::transform(get_attributes) | ranges::views::join | ranges::to<std::vector>();
}
void VertexBufferViewCollection::bind(vk::CommandBuffer &command_buffer) const {
    auto indexed_views_nooff = m_views | ranges::views::enumerate;
    auto indexed_views = indexed_views_nooff | ranges::views::transform([&](auto pr) {
                             auto [binding_index, view] = pr;
                             binding_index += m_first_binding;
                             return pr;
                         });

    for (const auto &[binding_index, view] : indexed_views) {

        auto [buf, off] = view.bind_info();
        if (buf) {
            command_buffer.bindVertexBuffers(m_first_binding, buf, off);
        }
    }
}
void VertexBufferViewCollection::draw(vk::CommandBuffer &command_buffer) const {
    bind(command_buffer);
    command_buffer.draw(m_vertex_count, m_instance_count, m_first_vertex, m_first_instance);
}
}// namespace balsa::visualization::vulkan::utils
