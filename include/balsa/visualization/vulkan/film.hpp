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

#if defined(USE_VULKAN_HPP)

    virtual vk::Format colorFormat() const = 0;

    virtual vk::CommandBuffer currentCommandBuffer() const = 0;
    virtual vk::Framebuffer currentFramebuffer() const = 0;

    virtual vk::RenderPass defaultRenderPass() const = 0;


    virtual vk::Format depthStencilFormat() const = 0;
    virtual vk::Image depthStencilImage() const = 0;
    virtual vk::ImageView depthStencilImageView() const = 0;


    virtual vk::Device device() const = 0;

    virtual void setPhysicalDeviceIndex(int index) = 0;
    virtual vk::PhysicalDevice physicalDevice() const = 0;
    virtual vk::PhysicalDeviceProperties physicalDeviceProperties() const = 0;

    virtual vk::CommandPool graphicsCommandPool() const = 0;


    virtual uint32_t graphicsQueueFamilyIndex() const = 0;
    virtual uint32_t hostVisibleMemoryIndex() const = 0;
    virtual vk::Queue graphicsQueue() const = 0;


    virtual vk::Image msaaColorImage(int index) const = 0;
    virtual vk::ImageView msaaColorImageView(int index) const = 0;


    virtual void setSampleCount(int sampleCount) = 0;
    virtual vk::SampleCountFlagBits sampleCountFlagBits() const = 0;
    virtual std::vector<int> supportedSampleCounts() = 0;


    virtual int swapChainImageCount() const = 0;
    virtual vk::Image swapChainImage(int index) const = 0;
    virtual vk::ImageView swapChainImageView(int index) const = 0;

#else

    virtual VkFormat colorFormat() const = 0;

    virtual VkCommandBuffer commandBuffer() const = 0;
    virtual VkFramebuffer framebuffer() const = 0;

    virtual VkRenderPass defaultRenderPass() const = 0;


    virtual VkFormat depthStencilFormat() const = 0;
    virtual VkImage depthStencilImage() const = 0;
    virtual VkImageView depthStencilImageView() const = 0;


    virtual VkDevice device() const = 0;

    virtual void setPhysicalDeviceIndex(int index) = 0;
    virtual VkPhysicalDevice physicalDevice() const = 0;
    virtual const VkPhysicalDeviceProperties *physicalDeviceProperties() const = 0;

    virtual VkCommandPool graphicsCommandPool() const = 0;


    virtual uint32_t graphicsQueueFamilyIndex() const = 0;
    virtual uint32_t hostVisibleMemoryIndex() const = 0;
    virtual VkQueue queue() const = 0;


    virtual VkImage msaaColorImage(int index) const = 0;
    virtual VkImageView msaaColorImageView(int index) const = 0;


    virtual void setSampleCount(int sampleCount) = 0;
    virtual VkSampleCountFlagBits sampleCountFlagBits() const = 0;
    virtual std::vector<int> supportedSampleCounts() = 0;


    virtual int swapChainImageCount() const = 0;
    virtual VkImage swapChainImage(int index) const = 0;
    virtual VkImageView swapChainImageView(int index) const = 0;

#endif
  private:
};
}// namespace balsa::visualization::vulkan

#endif
