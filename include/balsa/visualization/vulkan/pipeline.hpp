#if !defined(BALSA_VISUALIZATION_VULKAN_PIPELINE_HPP)
#define BALSA_VISUALIZATION_VULKAN_PIPELINE_HPP

#include <vector>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_enums.hpp>
#include <glm/vec2.hpp>

namespace balsa::visualization::vulkan {
    class Film;
    class Pipeline {
        public:

            void invoke(Film& film);
        private:
            vk::Pipeline _pipeline;
            vk::PipelineBindPoint _bind_point = vk::PipelineBindPoint::eGraphics;

    };
}

#endif
