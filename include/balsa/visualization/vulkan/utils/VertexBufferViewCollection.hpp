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
            void draw(vk::CommandBuffer &command_buffer) const;
            template<typename... T>
            VertexBufferView &emplace(T &&...args);


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

            void set_vertex_count(uint32_t vertex_count) { m_vertex_count = vertex_count; }
            void set_instance_count(uint32_t instance_count) { m_instance_count = instance_count; }
            void set_first_vertex(uint32_t first_vertex) { m_first_vertex = first_vertex; }
            void set_first_instance(uint32_t first_instance) { m_first_instance = first_instance; }

            std::vector<vk::VertexInputBindingDescription> binding_descriptions() const;
            std::vector<vk::VertexInputAttributeDescription> attribute_descriptions() const;

          private:
            std::vector<VertexBufferView> m_views;
            uint32_t m_first_binding = 0;

            uint32_t m_vertex_count = 0;
            uint32_t m_instance_count = 1;
            uint32_t m_first_vertex = 0;
            uint32_t m_first_instance = 0;


            // descsirptions are dirty if either of these descriptions are empty. if there are no views we are dirty as well
        };
        template<typename... T>
        VertexBufferView &VertexBufferViewCollection::emplace(T &&...args) {
            return m_views.emplace_back(std::forward<T>(args)...);
        }
    }// namespace utils
}// namespace vulkan
}// namespace balsa::visualization

#endif
