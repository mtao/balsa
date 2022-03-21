#include "balsa/visualization/qt/vulkan/film.hpp"
#include <QVulkanWindow>
#include <vulkan/vulkan_enums.hpp>
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


vk::CommandBuffer Film::current_command_buffer() const { return _window.currentCommandBuffer(); }

vk::Framebuffer Film::current_framebuffer() const { return _window.currentFramebuffer(); }

vk::RenderPass Film::default_render_pass() const { return _window.defaultRenderPass(); }


vk::Format Film::depthStencilFormat() const { return static_cast<vk::Format>(_window.depthStencilFormat()); }
vk::Image Film::depthStencilImage() const { return _window.depthStencilImage(); }
vk::ImageView Film::depthStencilImageView() const { return _window.depthStencilImageView(); }


vk::Device Film::device() const { return _window.device(); }

void Film::setPhysicalDeviceIndex(int index) { _window.setPhysicalDeviceIndex(index); }
vk::PhysicalDevice Film::physical_device() const { return _window.physicalDevice(); }
vk::PhysicalDeviceProperties Film::physicalDeviceProperties() const { return *_window.physicalDeviceProperties(); }

vk::CommandPool Film::graphicsCommandPool() const { return _window.graphicsCommandPool(); }


uint32_t Film::graphicsQueueFamilyIndex() const { return _window.graphicsQueueFamilyIndex(); }
uint32_t Film::hostVisibleMemoryIndex() const { return _window.hostVisibleMemoryIndex(); }
vk::Queue Film::graphicsQueue() const { return _window.graphicsQueue(); }


vk::Image Film::msaaColorImage(int index) const { return _window.msaaColorImage(index); }
vk::ImageView Film::msaaColorImageView(int index) const { return _window.msaaColorImageView(index); }


void Film::set_sample_count(vk::SampleCountFlagBits sampleCount) { _window.setSampleCount(int(sampleCount)); }

vk::SampleCountFlagBits Film::sample_count() const {
    return static_cast<vk::SampleCountFlagBits>(_window.sampleCountFlagBits());
}
vk::SampleCountFlags Film::supported_sample_counts() const {
    vk::SampleCountFlags flags = static_cast<vk::SampleCountFlags>(0);
    auto r = _window.supportedSampleCounts();
    for (auto &&v : r) {
        flags |= vk::SampleCountFlagBits(v);
    }
    return flags;
}


int Film::swapChainImageCount() const {
    return _window.swapChainImageCount();
}
vk::Image Film::swapChainImage(int index) const { return _window.swapChainImage(index); }
vk::ImageView Film::swapChainImageView(int index) const {
    return _window.swapChainImageView(index);
}
}// namespace balsa::visualization::qt::vulkan
