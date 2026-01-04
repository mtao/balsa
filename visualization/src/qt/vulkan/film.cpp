#include "balsa/visualization/qt/vulkan/film.hpp"
#include <QVulkanWindow>
#include <spdlog/spdlog.h>
#include <vulkan/vulkan_enums.hpp>
namespace balsa::visualization::qt::vulkan {
Film::Film(QVulkanWindow &window) : _window(window) {
    spdlog::info("First time calling myself on a window! {}", swapchain_image_count());
}
Film::~Film() {}

glm::uvec2 Film::swapchain_image_size() const {
    const QSize sz = _window.swapChainImageSize();
    return glm::uvec2(sz.width(), sz.height());
}


vk::Format Film::color_format() const {
    return static_cast<vk::Format>(_window.colorFormat());
}


vk::CommandBuffer Film::current_command_buffer() const { return _window.currentCommandBuffer(); }

vk::Framebuffer Film::current_framebuffer() const { return _window.currentFramebuffer(); }

vk::RenderPass Film::default_render_pass() const { return _window.defaultRenderPass(); }


vk::Format Film::depth_stencil_format() const { return static_cast<vk::Format>(_window.depthStencilFormat()); }
vk::Image Film::depth_stencil_image() const { return _window.depthStencilImage(); }
vk::ImageView Film::depth_stencil_image_view() const { return _window.depthStencilImageView(); }


vk::Device Film::device() const { return _window.device(); }

void Film::set_physical_device_index(int index) { _window.setPhysicalDeviceIndex(index); }
vk::PhysicalDevice Film::physical_device() const { return _window.physicalDevice(); }
vk::PhysicalDeviceProperties Film::physical_device_properties() const { return *_window.physicalDeviceProperties(); }

vk::CommandPool Film::graphics_command_pool() const { return _window.graphicsCommandPool(); }


uint32_t Film::graphics_queue_family_index() const { return _window.graphicsQueueFamilyIndex(); }
uint32_t Film::host_visible_memory_index() const { return _window.hostVisibleMemoryIndex(); }
vk::Queue Film::graphics_queue() const { return _window.graphicsQueue(); }


vk::Image Film::msaa_color_image(int index) const { return _window.msaaColorImage(index); }
vk::ImageView Film::msaa_color_image_view(int index) const { return _window.msaaColorImageView(index); }


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


uint32_t Film::swapchain_image_count() const {
    return _window.swapChainImageCount();
}
vk::Image Film::swapchain_image(int index) const { return _window.swapChainImage(index); }
vk::ImageView Film::swapchain_image_view(int index) const {
    return _window.swapChainImageView(index);
}
}// namespace balsa::visualization::qt::vulkan
