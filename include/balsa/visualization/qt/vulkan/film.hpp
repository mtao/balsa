#if !defined(BALSA_VISUALIZATION_QT_VULKAN_FILM_HPP)
#define BALSA_VISUALIZATION_QT_VULKAN_FILM_HPP

#include "balsa/visualization/vulkan/film.hpp"


class QVulkanWindow;
namespace balsa::visualization::qt::vulkan {
class Film : public visualization::vulkan::Film {
  public:
    Film(QVulkanWindow &window);
    ~Film();

    vk::Format colorFormat() const override;


    glm::uvec2 swapChainImageSize() const override;

    vk::CommandBuffer currentCommandBuffer() const override;
    vk::Framebuffer currentFramebuffer() const override;

    vk::RenderPass defaultRenderPass() const override;


    vk::Format depthStencilFormat() const override;
    vk::Image depthStencilImage() const override;
    vk::ImageView depthStencilImageView() const override;


    vk::Device device() const override;

    void setPhysicalDeviceIndex(int index) override;
    vk::PhysicalDevice physicalDevice() const override;
    vk::PhysicalDeviceProperties physicalDeviceProperties() const override;

    vk::CommandPool graphicsCommandPool() const override;


    uint32_t graphicsQueueFamilyIndex() const override;
    uint32_t hostVisibleMemoryIndex() const override;
    vk::Queue graphicsQueue() const override;


    vk::Image msaaColorImage(int index) const override;
    vk::ImageView msaaColorImageView(int index) const override;


    void setSampleCount(int sampleCount) override;
    vk::SampleCountFlagBits sampleCountFlagBits() const override;
    std::vector<int> supportedSampleCounts() override;


    int swapChainImageCount() const override;
    vk::Image swapChainImage(int index) const override;
    vk::ImageView swapChainImageView(int index) const override;

  private:
    QVulkanWindow &_window;
};
}// namespace balsa::visualization::qt::vulkan

#endif
