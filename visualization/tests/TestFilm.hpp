#pragma once

#include <algorithm>
#include <balsa/visualization/vulkan/film.hpp>

namespace balsa::visualization::vulkan::test {

// Device and queue for Vulkan tests without a window or swapchain.
class TestFilm : public Film {
  public:
    explicit TestFilm(uint32_t api_version = VK_API_VERSION_1_0) {
        const auto application =
            vk::ApplicationInfo{}.setApiVersion(api_version);
        _instance = vk::createInstanceUnique(
            vk::InstanceCreateInfo{}.setPApplicationInfo(&application));
        const auto devices = _instance->enumeratePhysicalDevices();
        if (devices.empty())
            throw std::runtime_error("No Vulkan device available");
        _physical = devices.front();
        if (_physical.getProperties().apiVersion < api_version) {
            throw std::runtime_error(
                "Requested Vulkan API version unavailable");
        }
        const auto families = _physical.getQueueFamilyProperties();
        const auto family =
            std::ranges::find_if(families, [](const auto &properties) {
                return bool(properties.queueFlags
                            & vk::QueueFlagBits::eGraphics);
            });
        if (family == families.end())
            throw std::runtime_error("No graphics queue available");
        _family = static_cast<uint32_t>(family - families.begin());
        const float priority = 1.0f;
        const auto queue = vk::DeviceQueueCreateInfo{}
                               .setQueueFamilyIndex(_family)
                               .setQueuePriorities(priority);
        _device = _physical.createDeviceUnique(
            vk::DeviceCreateInfo{}.setQueueCreateInfos(queue));
        _pool = _device->createCommandPoolUnique(
            vk::CommandPoolCreateInfo{}.setQueueFamilyIndex(_family));
    }
    ~TestFilm() override {
        _device->waitIdle();
        release_retired();
        release_buffer_allocator();
    }

    auto instance() const -> vk::Instance override { return *_instance; }
    auto device() const -> vk::Device override { return *_device; }
    auto physical_device() const -> vk::PhysicalDevice override {
        return _physical;
    }
    auto physical_device_properties() const
        -> vk::PhysicalDeviceProperties override {
        return _physical.getProperties();
    }
    auto graphics_command_pool() const -> vk::CommandPool override {
        return *_pool;
    }
    auto graphics_queue_family_index() const -> uint32_t override {
        return _family;
    }
    auto graphics_queue() const -> vk::Queue override {
        return _device->getQueue(_family, 0);
    }
    auto host_visible_memory_index() const -> uint32_t override {
        return find_memory_type(~uint32_t{0},
                                vk::MemoryPropertyFlagBits::eHostVisible);
    }
    void set_physical_device_index(int) override {}
    auto swapchain_image_size() const -> std::array<uint32_t, 2> override {
        return {1, 1};
    }
    auto color_format() const -> vk::Format override {
        return vk::Format::eUndefined;
    }
    auto current_command_buffer() const -> vk::CommandBuffer override {
        return {};
    }
    auto current_framebuffer() const -> vk::Framebuffer override { return {}; }
    auto default_render_pass() const -> vk::RenderPass override { return {}; }
    auto depth_stencil_format() const -> vk::Format override {
        return vk::Format::eUndefined;
    }
    auto depth_stencil_image() const -> vk::Image override { return {}; }
    auto depth_stencil_image_view() const -> vk::ImageView override {
        return {};
    }
    auto msaa_color_image(int) const -> vk::Image override { return {}; }
    auto msaa_color_image_view(int) const -> vk::ImageView override {
        return {};
    }
    void set_sample_count(vk::SampleCountFlagBits) override {}
    auto sample_count() const -> vk::SampleCountFlagBits override {
        return vk::SampleCountFlagBits::e1;
    }
    auto supported_sample_counts() const -> vk::SampleCountFlags override {
        return vk::SampleCountFlagBits::e1;
    }
    auto swapchain_image_count() const -> uint32_t override { return 0; }
    auto swapchain_image(int) const -> vk::Image override { return {}; }
    auto swapchain_image_view(int) const -> vk::ImageView override {
        return {};
    }
    auto concurrent_frame_count() const -> int override { return 2; }
    auto current_frame() const -> int override { return 0; }

  private:
    vk::UniqueInstance _instance;
    vk::PhysicalDevice _physical;
    vk::UniqueDevice _device;
    vk::UniqueCommandPool _pool;
    uint32_t _family = 0;
};

} // namespace balsa::visualization::vulkan::test
