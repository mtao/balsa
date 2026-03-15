#if !defined(BALSA_VISUALIZATION_VULKAN_PIPELINE_HPP)
#define BALSA_VISUALIZATION_VULKAN_PIPELINE_HPP

#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_enums.hpp>

namespace balsa::visualization::vulkan {
class Film;

class Pipeline {
  public:
    Pipeline() = default;
    Pipeline(vk::Device device, vk::Pipeline pipeline, vk::PipelineBindPoint bind_point = vk::PipelineBindPoint::eGraphics);
    ~Pipeline();

    // Non-copyable (raw Vulkan handle ownership)
    Pipeline(const Pipeline &) = delete;
    Pipeline &operator=(const Pipeline &) = delete;

    // Movable
    Pipeline(Pipeline &&other) noexcept;
    Pipeline &operator=(Pipeline &&other) noexcept;

    void invoke(Film &film);

    // Explicitly release the Vulkan pipeline.  Safe to call multiple times.
    void release();

  private:
    vk::Device _device;
    vk::Pipeline _pipeline;
    vk::PipelineBindPoint _bind_point = vk::PipelineBindPoint::eGraphics;
};
}// namespace balsa::visualization::vulkan

#endif
