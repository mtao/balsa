
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
    set_dirty_descriptions();
    m_views.resize(size);
}
void VertexBufferViewCollection::add_view(const VertexBufferView &view) {
    m_views.emplace_back(view);
}
void VertexBufferViewCollection::set_dirty_descriptions() {
    m_binding_descriptions.clear();
    m_attribute_descriptions.clear();
}
bool VertexBufferViewCollection::dirty_descriptions() const {
    return m_binding_descriptions.empty() || m_attribute_descriptions.empty();
}

void VertexBufferViewCollection::set_vertex_inputs(PipelineFactory &factory) const {
    set_vertex_inputs(factory.vertex_input);
}

void VertexBufferViewCollection::set_vertex_inputs(vk::PipelineVertexInputStateCreateInfo &create_info) const {
    create_info.setVertexBindingDescriptions(m_binding_descriptions);
    create_info.setVertexAttributeDescriptions(m_attribute_descriptions);
}
vk::PipelineVertexInputStateCreateInfo VertexBufferViewCollection::pipeline_vertex_input_state_create_info() const {
    vk::PipelineVertexInputStateCreateInfo ret;
    set_vertex_inputs(ret);
    return ret;
}

void VertexBufferViewCollection::build_descriptions() {

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

    m_binding_descriptions = indexed_views | ranges::views::transform(get_binding) | ranges::to<std::vector>();


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


    m_attribute_descriptions = indexed_views | ranges::views::transform(get_attributes) | ranges::views::join | ranges::to<std::vector>();
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
}// namespace balsa::visualization::vulkan::utils
