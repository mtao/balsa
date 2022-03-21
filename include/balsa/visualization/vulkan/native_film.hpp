#if !defined(BALSA_VISUALIZATION_VULKAN_NATIVE_FILM_HPP)
#define BALSA_VISUALIZATION_VULKAN_NATIVE_FILM_HPP

#include <vulkan/vulkan_raii.hpp>
#include <set>
#include "film.hpp"
#include "device_functions.hpp"

namespace balsa::visualization::vulkan {

class NativeFilm : public Film {
  public:
    NativeFilm(const std::vector<std::string> &device_extensions = {}, const std::vector<std::string> &validation_layers = {});
    void set_device_extensions(const std::vector<std::string> &device_extensions);
    void set_validation_layers(const std::vector<std::string> &validation_layers);
    NativeFilm(std::nullptr_t);
    virtual ~NativeFilm();
    glm::uvec2 swapChainImageSize() const override;


    vk::Format colorFormat() const override;

    vk::CommandBuffer current_command_buffer() const override;
    vk::Framebuffer current_framebuffer() const override;

    vk::RenderPass default_render_pass() const override;


    vk::Format depthStencilFormat() const override;
    vk::Image depthStencilImage() const override;
    vk::ImageView depthStencilImageView() const override;


    vk::Device device() const override;
    const vk::raii::Device &device_raii() const;
    vk::SurfaceKHR surface() const;

    void setPhysicalDeviceIndex(int index) override;
    vk::PhysicalDevice physical_device() const override;
    const vk::raii::PhysicalDevice &physical_device_raii() const;
    vk::PhysicalDeviceProperties physicalDeviceProperties() const override;

    vk::CommandPool graphicsCommandPool() const override;


    uint32_t graphicsQueueFamilyIndex() const override;
    uint32_t hostVisibleMemoryIndex() const override;
    vk::Queue graphicsQueue() const override;


    vk::Image msaaColorImage(int index) const override;
    vk::ImageView msaaColorImageView(int index) const override;


    void set_sample_count(vk::SampleCountFlagBits) override;
    vk::SampleCountFlagBits sample_count() const override;
    vk::SampleCountFlags supported_sample_counts() const override;


    int swapChainImageCount() const override;
    vk::Image swapChainImage(int index) const override;
    vk::ImageView swapChainImageView(int index) const override;

    vk::Instance instance() const;
    const vk::raii::Instance &instance_raii() const;

    void pre_draw();

    void post_draw();

  protected:
    // Whatever window management tool we have is in charge of this
    virtual vk::raii::SurfaceKHR make_surface() const = 0;
    virtual vk::Extent2D choose_swapchain_extent() const = 0;
    void initialize();

  private:
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

    bool check_validation_layer_support();
    bool enable_validation_layers = true;
    virtual std::vector<std::string> get_required_extensions();
    vk::DebugUtilsMessengerCreateInfoEXT debug_utils_messenger_create_info() const;

    void create_debug_messenger();


    QueueTargetIndices available_queues(const vk::PhysicalDevice &device) const;

    std::vector<std::string> _device_extensions;
    std::vector<std::string> _validation_layers;


    // Structure borrowed from QVulkanWindow
    struct ImageResources {
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

    uint32_t _current_swapchain_index = 0;
    uint32_t _current_frame_index = 0;
    void create_image_resources();
    std::vector<ImageResources> _image_resources;
    void create_frame_resources();
    std::vector<FrameResources> _frame_resources;


    void create_render_pass();
    vk::raii::RenderPass _default_render_pass_raii = nullptr;
};
}// namespace balsa::visualization::vulkan
#endif

