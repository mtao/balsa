
#include "balsa/visualization/vulkan/pipeline.hpp"
#include "balsa/visualization/vulkan/Film.hpp"
#include <vulkan/vulkan.hpp>


namespace balsa::visualization::vulkan {
    void Pipeline::invoke(Film& film) {

        vk::CommandBuffer cmd_buffer = film.commandBuffer();


        cmd_buffer.bindPipeline(_bind_point, _pipeline);


    }
}
