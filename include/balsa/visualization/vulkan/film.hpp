#if !defined(BALSA_VISUALIZATION_VULKAN_FILM_HPP)
#define BALSA_VISUALIZATION_VULKAN_FILM_HPP

#include <vector>
#include <vulkan/vulkan_core.h>
#include <vulkan/vulkan.hpp>
#include <glm/vec2.hpp>
#define USE_VULKAN_HPP

namespace balsa::visualization::vulkan {
namespace objects {
    class Node;
}

class Film {
  public:
    virtual ~Film();

    virtual glm::uvec2 swapChainImageSize() const = 0;

    virtual vk::Format colorFormat() const = 0;

    virtual vk::CommandBuffer current_command_buffer() const = 0;
    virtual vk::Framebuffer current_framebuffer() const = 0;

    virtual vk::RenderPass default_render_pass() const = 0;


    virtual vk::Format depthStencilFormat() const = 0;
    virtual vk::Image depthStencilImage() const = 0;
    virtual vk::ImageView depthStencilImageView() const = 0;


    virtual vk::Device device() const = 0;

    virtual void setPhysicalDeviceIndex(int index) = 0;
    virtual vk::PhysicalDevice physical_device() const = 0;
    virtual vk::PhysicalDeviceProperties physicalDeviceProperties() const = 0;

    virtual vk::CommandPool graphicsCommandPool() const = 0;


    virtual uint32_t graphicsQueueFamilyIndex() const = 0;
    virtual uint32_t hostVisibleMemoryIndex() const = 0;
    virtual vk::Queue graphicsQueue() const = 0;


    virtual vk::Image msaaColorImage(int index) const = 0;
    virtual vk::ImageView msaaColorImageView(int index) const = 0;


    virtual void set_sample_count(vk::SampleCountFlagBits) = 0;
    virtual vk::SampleCountFlagBits sample_count() const = 0;
    virtual vk::SampleCountFlags supported_sample_counts() const = 0;


    virtual int swapChainImageCount() const = 0;
    virtual vk::Image swapChainImage(int index) const = 0;
    virtual vk::ImageView swapChainImageView(int index) const = 0;

  private:
};
}// namespace balsa::visualization::vulkan

#endif
