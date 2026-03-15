
#include "balsa/visualization/vulkan/pipeline.hpp"
#include "balsa/visualization/vulkan/film.hpp"
#include <vulkan/vulkan.hpp>

namespace balsa::visualization::vulkan {

Pipeline::Pipeline(vk::Device device, vk::Pipeline pipeline, vk::PipelineBindPoint bind_point)
  : _device(device), _pipeline(pipeline), _bind_point(bind_point) {}

Pipeline::~Pipeline() {
    release();
}

Pipeline::Pipeline(Pipeline &&other) noexcept
  : _device(other._device), _pipeline(other._pipeline), _bind_point(other._bind_point) {
    other._device = vk::Device{};
    other._pipeline = vk::Pipeline{};
}

Pipeline &Pipeline::operator=(Pipeline &&other) noexcept {
    if (this != &other) {
        release();
        _device = other._device;
        _pipeline = other._pipeline;
        _bind_point = other._bind_point;
        other._device = vk::Device{};
        other._pipeline = vk::Pipeline{};
    }
    return *this;
}

void Pipeline::release() {
    if (_device && _pipeline) {
        _device.destroyPipeline(_pipeline);
        _pipeline = vk::Pipeline{};
    }
}

void Pipeline::invoke(Film &film) {
    vk::CommandBuffer cmd_buffer = film.current_command_buffer();
    cmd_buffer.bindPipeline(_bind_point, _pipeline);
}

}// namespace balsa::visualization::vulkan
