#if !defined(BALSA_VISUALIZATION_VULKAN_UTILS_VERTEX_BUFFER_VIEW_COLLECTION_HPP)
#define BALSA_VISUALIZATION_VULKAN_UTILS_VERTEX_BUFFER_VIEW_COLLECTION_HPP

#include <vulkan/vulkan.hpp>
#include "../BufferView.hpp"


namespace balsa::visualization {
namespace vulkan {
    class Film;
    namespace utils {
        class PipelineFactory;

        class VertexBufferViewCollection {
          public:
            void clear();
            void resize(size_t size);
            void add_view(const VertexBufferView &);
            void bind(vk::CommandBuffer &command_buffer) const;
            template<typename... T>
            VertexBufferView &emplace(T &&...args);

            void set_vertex_inputs(PipelineFactory &factory) const;
            void set_vertex_inputs(vk::PipelineVertexInputStateCreateInfo &ci) const;
            vk::PipelineVertexInputStateCreateInfo pipeline_vertex_input_state_create_info() const;


            bool dirty_descriptions() const;

            std::span<VertexBufferView> views() {
                return m_views;
            }
            std::span<const VertexBufferView> views() const {
                return m_views;
            }

            void set_first_binding(uint32_t value) {
                m_first_binding = value;
            }
            uint32_t first_binding() const {
                return m_first_binding;
            }
            void build_descriptions();


          private:
            std::vector<VertexBufferView> m_views;
            uint32_t m_first_binding = 0;

            void set_dirty_descriptions();

            // descsirptions are dirty if either of these descriptions are empty. if there are no views we are dirty as well
            std::vector<vk::VertexInputBindingDescription> m_binding_descriptions;
            std::vector<vk::VertexInputAttributeDescription> m_attribute_descriptions;
        };
        template<typename... T>
        VertexBufferView &VertexBufferViewCollection::emplace(T &&...args) {
            set_dirty_descriptions();
            return m_views.emplace_back(std::forward<T>(args)...);
        }
    }// namespace utils
}// namespace vulkan
}// namespace balsa::visualization

#endif
