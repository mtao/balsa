#if !defined(BALSA_VISUALIZATION_QT_VULKAN_FILM_HPP)
#define BALSA_VISUALIZATION_QT_VULKAN_FILM_HPP

#include "balsa/visualization/vulkan/film.hpp"


class QVulkanWindow;
namespace balsa::visualization::qt::vulkan {
class Film : public visualization::vulkan::Film {
  public:
    Film(QVulkanWindow &window);
    ~Film();

    vk::Format color_format() const override;


    glm::uvec2 swapchain_image_size() const override;

    vk::CommandBuffer current_command_buffer() const override;
    vk::Framebuffer current_framebuffer() const override;

    vk::RenderPass default_render_pass() const override;


    vk::Format depth_stencil_format() const override;
    vk::Image depth_stencil_image() const override;
    vk::ImageView depth_stencil_image_view() const override;


    vk::Device device() const override;

    void set_physical_device_index(int index) override;
    vk::PhysicalDevice physical_device() const override;
    vk::PhysicalDeviceProperties physical_device_properties() const override;

    vk::CommandPool graphics_command_pool() const override;


    uint32_t graphics_queue_family_index() const override;
    uint32_t host_visible_memory_index() const override;
    vk::Queue graphics_queue() const override;


    vk::Image msaa_color_image(int index) const override;
    vk::ImageView msaa_color_image_view(int index) const override;

    void set_sample_count(vk::SampleCountFlagBits) override;
    vk::SampleCountFlagBits sample_count() const override;
    vk::SampleCountFlags supported_sample_counts() const override;


    uint32_t swapchain_image_count() const override;
    vk::Image swapchain_image(int index) const override;
    vk::ImageView swapchain_image_view(int index) const override;

  private:
    QVulkanWindow &_window;
};
}// namespace balsa::visualization::qt::vulkan

#endif
