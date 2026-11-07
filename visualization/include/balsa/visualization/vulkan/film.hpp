#pragma once
#include <array>
#include <cstdint>
#include <concepts>
#include <memory>
#include <vector>
#include <vulkan/vulkan_core.h>
#include <vulkan/vulkan.hpp>

namespace balsa::visualization::vulkan {

class Film {
  public:
    Film() = default;
    Film(const Film &) = delete;
    auto operator=(const Film &) -> Film & = delete;
    Film(Film &&) = default;
    auto operator=(Film &&) -> Film & = default;
    virtual ~Film();

    virtual std::array<uint32_t, 2> swapchain_image_size() const = 0;

    virtual vk::Format color_format() const = 0;

    virtual vk::CommandBuffer current_command_buffer() const = 0;
    virtual vk::Framebuffer current_framebuffer() const = 0;

    virtual vk::RenderPass default_render_pass() const = 0;


    virtual vk::Format depth_stencil_format() const = 0;
    virtual vk::Image depth_stencil_image() const = 0;
    virtual vk::ImageView depth_stencil_image_view() const = 0;


    virtual vk::Instance instance() const = 0;
    virtual vk::Device device() const = 0;

    virtual void set_physical_device_index(int index) = 0;
    virtual vk::PhysicalDevice physical_device() const = 0;
    virtual vk::PhysicalDeviceProperties physical_device_properties() const = 0;

    virtual vk::CommandPool graphics_command_pool() const = 0;


    virtual uint32_t graphics_queue_family_index() const = 0;
    virtual uint32_t host_visible_memory_index() const = 0;
    virtual vk::Queue graphics_queue() const = 0;

    // Find a memory type index that satisfies both the type_filter bitmask
    // (from VkMemoryRequirements::memoryTypeBits) and the desired property flags.
    // Throws std::runtime_error if no suitable memory type is found.
    uint32_t find_memory_type(uint32_t type_filter, vk::MemoryPropertyFlags properties) const;


    virtual vk::Image msaa_color_image(int index) const = 0;
    virtual vk::ImageView msaa_color_image_view(int index) const = 0;


    virtual void set_sample_count(vk::SampleCountFlagBits) = 0;
    virtual vk::SampleCountFlagBits sample_count() const = 0;
    virtual vk::SampleCountFlags supported_sample_counts() const = 0;

    virtual void set_depth_stencil_format(vk::Format) {}
    virtual bool has_depth_stencil() const { return depth_stencil_format() != vk::Format::eUndefined; }


    virtual uint32_t swapchain_image_count() const = 0;
    virtual vk::Image swapchain_image(int index) const = 0;
    virtual vk::ImageView swapchain_image_view(int index) const = 0;

    virtual void pre_draw_hook() {}
    virtual void post_draw_hook() {}

    // ── Frames-in-flight ────────────────────────────────────────────
    //
    // concurrent_frame_count() returns the number of frames the backend
    // allows to be in-flight simultaneously (QVulkanWindow defaults to 2;
    // NativeFilm defaults to 1).  current_frame() returns a 0-based index
    // in [0, concurrent_frame_count()) identifying which frame slot is
    // currently being recorded.  Per-frame GPU resources (UBOs, descriptor
    // sets) must be duplicated for each slot so that the CPU can upload
    // data for the current frame without clobbering data the GPU is still
    // reading from a prior frame.
    virtual int concurrent_frame_count() const = 0;
    virtual int current_frame() const = 0;

    // Whether VK_KHR_fragment_shader_barycentric is enabled on this device.
    // Used by the wireframe overlay path to access gl_BaryCoordEXT in the
    // fragment shader.  Default: false (line-based wireframe fallback).
    virtual bool has_fragment_shader_barycentric() const { return false; }

    // ── Deferred destruction ────────────────────────────────────────
    //
    // retire() keeps `resource` alive until every frame now in flight has
    // finished, then destroys it; its destructor frees its GPU objects.
    // collect_retired() advances the frame count and destroys what is due;
    // call it once per frame, after the frame slot's fence wait.
    template <typename T>
        requires std::movable<T> && std::destructible<T>
    void retire(T resource) {
        _retired.push_back({_frame_number, std::make_shared<T>(std::move(resource))});
    }
    void collect_retired();
    // Destroys everything retired; the GPU must be idle.  Films that own
    // their device call this before destroying it.
    void release_retired();

  private:
    struct Retired {
        uint64_t frame;
        std::shared_ptr<void> resource;
    };
    uint64_t _frame_number = 0;
    std::vector<Retired> _retired;
};
}// namespace balsa::visualization::vulkan
