#if !defined(BALSA_VISUALIZATION_VULKAN_NATIVE_FILM_HPP)
#define BALSA_VISUALIZATION_VULKAN_NATIVE_FILM_HPP

#include <vulkan/vulkan_raii.hpp>
#include <set>
#include "balsa/visualization/vulkan/film.hpp"
#include "device_functions.hpp"

namespace balsa::visualization::vulkan {

class NativeFilm : public Film {
  public:
    NativeFilm(const std::vector<std::string> &device_extensions = {}, const std::vector<std::string> &instance_extensions = {}, const std::vector<std::string> &validation_layers = {});
    NativeFilm(NativeFilm&&) = default;
    NativeFilm& operator=(NativeFilm&&) = default;
    void set_device_extensions(const std::vector<std::string> &device_extensions);
    void set_instance_extensions(const std::vector<std::string> &device_extensions);
    void set_validation_layers(const std::vector<std::string> &validation_layers);
    NativeFilm(std::nullptr_t);
    virtual ~NativeFilm() override;

    glm::uvec2 swapchain_image_size() const override;


    vk::Format color_format() const override;

    vk::CommandBuffer current_command_buffer() const override;
    vk::Framebuffer current_framebuffer() const override;

    vk::RenderPass default_render_pass() const override;


    vk::Format depth_stencil_format() const override;
    vk::Image depth_stencil_image() const override;
    vk::ImageView depth_stencil_image_view() const override;


    vk::Device device() const override;
    const vk::raii::Device &device_raii() const;
    vk::SurfaceKHR surface() const;

    void set_physical_device_index(int index) override;
    vk::PhysicalDevice physical_device() const override;
    const vk::raii::PhysicalDevice &physical_device_raii() const;
    vk::PhysicalDeviceProperties physical_device_properties() const override;

    vk::CommandPool graphics_command_pool() const override;


    uint32_t graphics_queue_family_index() const override;
    uint32_t host_visible_memory_index() const override;
    vk::Queue graphics_queue() const override;
    uint32_t present_queue_family_index() const;
    vk::Queue present_queue() const;


    vk::Image msaa_color_image(int index) const override;
    vk::ImageView msaa_color_image_view(int index) const override;


    void set_sample_count(vk::SampleCountFlagBits) override;
    vk::SampleCountFlagBits sample_count() const override;
    vk::SampleCountFlags supported_sample_counts() const override;


    uint32_t swapchain_image_count() const override;
    vk::Image swapchain_image(int index) const override;
    vk::ImageView swapchain_image_view(int index) const override;
    const vk::Extent2D &swapchain_extent() const;
    const vk::SurfaceFormatKHR surface_format() const;

    vk::Instance instance() const;
    const vk::raii::Instance &instance_raii() const;

    void pre_draw_hook() override;

    void post_draw_hook() override;

    // protected:
    //  Whatever window management tool we have is in charge of this
    virtual vk::raii::SurfaceKHR make_surface() const = 0;
    virtual vk::Extent2D choose_swapchain_extent() const = 0;
    const vk::SwapchainKHR &swapchain() const;
    void initialize();

    virtual std::vector<std::string> get_required_instance_extensions() const;
    virtual std::vector<std::string> get_required_device_extensions() const;


    // private:
    struct QueueTargetIndices {
        std::set<uint32_t> graphics_queue;
        std::set<uint32_t> compute_queue;
        std::set<uint32_t> transfer_queue;
        std::set<uint32_t> sparse_binding_queue;
        // std::optional<uint32_t> video_decode_queue;
        // std::optional<uint32_t> video_encode_queue;
        std::set<uint32_t> protected_queue;
        std::set<uint32_t> present_queue;

        QueueTargetIndices(const vk::PhysicalDevice &device,
                           vk::SurfaceKHR const *surface = nullptr);

        vk::QueueFlags active_flags() const;
        bool meets_requirements(vk::QueueFlags requirement) const;
    };

    // Debug and validation functionality
    bool check_validation_layer_support();
    bool enable_validation_layers = true;
    vk::DebugUtilsMessengerCreateInfoEXT debug_utils_messenger_create_info() const;

    void create_debug_messenger();


    QueueTargetIndices available_queues(const vk::PhysicalDevice &device) const;

    // Extensions that are specified at construction / before init
    std::vector<std::string> _device_extensions;
    std::vector<std::string> _instance_extensions;
    std::vector<std::string> _validation_layers;


    // Structure borrowed from QVulkanWindow
    struct ImageResources {
        // the Image being rendered to
        vk::Image image = nullptr;
        vk::raii::ImageView image_view_raii = nullptr;
        vk::raii::CommandBuffer command_buffer_raii = nullptr;
        vk::raii::Fence command_fence_raii = nullptr;
        bool command_fence_waitable = false;
        vk::raii::Framebuffer framebuffer_raii = nullptr;
        vk::raii::CommandBuffer present_command_buffer_raii = nullptr;
        // vk::Image msaa_image = nullptr;
        // vk::raii::Image msaa_image_view = nullptr;
    };

    // Structure borrowed from QVulkanWindow
    struct FrameResources {
        // make sure we dont mainpulate the same frame more than once
        vk::raii::Fence fence_raii = nullptr;
        bool fence_waitable = false;

        vk::raii::Semaphore image_semaphore_raii = nullptr;
        vk::raii::Semaphore draw_semaphore_raii = nullptr;
        vk::raii::Semaphore present_semaphore_raii = nullptr;
        bool image_acquired = false;
        bool image_semaphore_waitable = false;
    };


    vk::raii::Context _context_raii;

    void create_instance();
    vk::raii::Instance _instance_raii = nullptr;
    vk::raii::DebugUtilsMessengerEXT _debug_messenger_raii = nullptr;

    virtual void pick_physical_device();
    virtual int score_physical_devices(const vk::PhysicalDevice &device) const;
    vk::raii::PhysicalDevice _physical_device_raii = nullptr;

    void create_device();
    vk::raii::Device _device_raii = nullptr;
    uint32_t _graphics_queue_family_index;
    uint32_t _present_queue_family_index;
    vk::raii::Queue _graphics_queue_raii = nullptr;
    vk::raii::Queue _present_queue_raii = nullptr;

    // make_surface()
    vk::raii::SurfaceKHR _surface_raii = nullptr;


    // choose_swapchain_extent()
    uint32_t choose_swapchain_image_count() const;
    void create_swapchain();
    vk::Extent2D _swapchain_extent;
    vk::SurfaceFormatKHR _surface_format;
    vk::raii::SwapchainKHR _swapchain_raii = nullptr;
    size_t _swapchain_count = 5;

    void create_graphics_command_pool();
    vk::raii::CommandPool _graphics_command_pool_raii = nullptr;


    size_t _frame_count = 1;

    uint32_t _current_image_index = 0;
    uint32_t _current_frame_index = 0;
    void create_image_resources();
    std::vector<ImageResources> _image_resources;
    void create_frame_resources();
    std::vector<FrameResources> _frame_resources;


    void create_render_pass();
    vk::raii::RenderPass _default_render_pass_raii = nullptr;

  public:
    void set_swapchain_index(uint32_t index) { _current_image_index = index; }
    const ImageResources &image_resources() const { return _image_resources[_current_image_index]; }
    const FrameResources &frame_resources() const { return _frame_resources[0]; }
    const ImageResources &image_resources(size_t index) const { return _image_resources.at(index); }
    const FrameResources &frame_resources(size_t index) const { return _frame_resources.at(index); }
};
}// namespace balsa::visualization::vulkan
#endif

