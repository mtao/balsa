#if !defined(BALSA_VISUALIZATION_VULKAN_FILM_HPP)
#define BALSA_VISUALIZATION_VULKAN_FILM_HPP

#include <vector>
#include <vulkan/vulkan_core.h>
#include <vulkan/vulkan.hpp>
#include <glm/vec2.hpp>

namespace balsa::visualization::vulkan {

class Film {
  public:
    virtual ~Film();

    virtual glm::uvec2 swapchain_image_size() const = 0;

    virtual vk::Format color_format() const = 0;

    virtual vk::CommandBuffer current_command_buffer() const = 0;
    virtual vk::Framebuffer current_framebuffer() const = 0;

    virtual vk::RenderPass default_render_pass() const = 0;


    virtual vk::Format depth_stencil_format() const = 0;
    virtual vk::Image depth_stencil_image() const = 0;
    virtual vk::ImageView depth_stencil_image_view() const = 0;


    virtual vk::Device device() const = 0;

    virtual void set_physical_device_index(int index) = 0;
    virtual vk::PhysicalDevice physical_device() const = 0;
    virtual vk::PhysicalDeviceProperties physical_device_properties() const = 0;

    virtual vk::CommandPool graphics_command_pool() const = 0;


    virtual uint32_t graphics_queue_family_index() const = 0;
    virtual uint32_t host_visible_memory_index() const = 0;
    virtual vk::Queue graphics_queue() const = 0;


    virtual vk::Image msaa_color_image(int index) const = 0;
    virtual vk::ImageView msaa_color_image_view(int index) const = 0;


    virtual void set_sample_count(vk::SampleCountFlagBits) = 0;
    virtual vk::SampleCountFlagBits sample_count() const = 0;
    virtual vk::SampleCountFlags supported_sample_counts() const = 0;


    virtual uint32_t swapchain_image_count() const = 0;
    virtual vk::Image swapchain_image(int index) const = 0;
    virtual vk::ImageView swapchain_image_view(int index) const = 0;

    virtual void pre_draw_hook() {}
    virtual void post_draw_hook() {}


  private:
};
}// namespace balsa::visualization::vulkan

#endif
