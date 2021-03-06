#include "balsa/visualization/qt/vulkan/film.hpp"
#include <QVulkanWindow>
namespace balsa::visualization::qt::vulkan {
Film::Film(QVulkanWindow &window) : _window(window) {}
Film::~Film() {}

glm::uvec2 Film::swapChainImageSize() const {
    const QSize sz = _window.swapChainImageSize();
    return glm::uvec2(sz.width(), sz.height());
}


vk::Format Film::colorFormat() const {
    return static_cast<vk::Format>(_window.colorFormat());
}


vk::CommandBuffer Film::commandBuffer() const { return _window.currentCommandBuffer(); }

vk::Framebuffer Film::framebuffer() const { return _window.currentFramebuffer(); }

vk::RenderPass Film::defaultRenderPass() const { return _window.defaultRenderPass(); }


vk::Format Film::depthStencilFormat() const { return static_cast<vk::Format>(_window.depthStencilFormat()); }
vk::Image Film::depthStencilImage() const { return _window.depthStencilImage(); }
vk::ImageView Film::depthStencilImageView() const { return _window.depthStencilImageView(); }


vk::Device Film::device() const { return _window.device(); }

void Film::setPhysicalDeviceIndex(int index) { _window.setPhysicalDeviceIndex(index); }
vk::PhysicalDevice Film::physicalDevice() const { return _window.physicalDevice(); }
vk::PhysicalDeviceProperties Film::physicalDeviceProperties() const { return *_window.physicalDeviceProperties(); }

vk::CommandPool Film::graphicsCommandPool() const { return _window.graphicsCommandPool(); }


uint32_t Film::graphicsQueueFamilyIndex() const { return _window.graphicsQueueFamilyIndex(); }
uint32_t Film::hostVisibleMemoryIndex() const { return _window.hostVisibleMemoryIndex(); }
vk::Queue Film::queue() const { return _window.graphicsQueue(); }


vk::Image Film::msaaColorImage(int index) const { return _window.msaaColorImage(index); }
vk::ImageView Film::msaaColorImageView(int index) const { return _window.msaaColorImageView(index); }


void Film::setSampleCount(int sampleCount) { _window.setSampleCount(sampleCount); }
vk::SampleCountFlagBits Film::sampleCountFlagBits() const {
    return static_cast<vk::SampleCountFlagBits>(_window.sampleCountFlagBits());
}
std::vector<int> Film::supportedSampleCounts() {
    auto r = _window.supportedSampleCounts();
    std::vector<int> ret(r.begin(), r.end());
    return ret;
}


int Film::swapChainImageCount() const {
    return _window.swapChainImageCount();
}
vk::Image Film::swapChainImage(int index) const { return _window.swapChainImage(index); }
vk::ImageView Film::swapChainImageView(int index) const {
    return _window.swapChainImageView(index);
}
}// namespace balsa::visualization::qt::vulkan
