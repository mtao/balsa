#if !defined(BALSA_VISUALIZATION_VULKAN_NATIVE_FILM_HPP)
#define BALSA_VISUALIZATION_VULKAN_NATIVE_FILM_HPP

#include <vulkan/vulkan_raii.hpp>
#include <set>
#include "film.hpp"

namespace balsa::visualization::vulkan {

class NativeFilm : public Film {
  public:
    glm::uvec2 swapChainImageSize() const override;


    vk::Format colorFormat() const override;

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

    bool checkValidationLayerSupport();
    virtual std::vector<const char *> getRequiredExtensions();
    vk::DebugUtilsMessengerCreateInfoEXT debug_utils_messenger_create_info() const;

    void create_instance();
    void create_debug_messenger();
    virtual void pick_physical_device();
    virtual void create_device() = 0;

    // Whatever window management tool we have is in charge of this
    virtual void create_surface() = 0;

    QueueTargetIndices available_queues(const vk::PhysicalDevice &device) const;
    virtual int score_physical_devices(const vk::PhysicalDevice &device) const;

    static const std::vector<const char *> validation_layers;


    // Structure borrowed from QVulkanWindow
    struct ImageResources {
        vk::Image image = nullptr;
        vk::raii::ImageView image_view_raii = nullptr;
        vk::CommandBuffer command_buffer = nullptr;
        vk::Fence command_fence = nullptr;
        bool command_fence_waitable = false;
        vk::raii::Framebuffer framebuffer_raii = nullptr;
        vk::CommandBuffer present_command_buffer = nullptr;
        // vk::Image msaa_image = nullptr;
        // vk::raii::Image msaa_image_view = nullptr;
    };

    // Structure borrowed from QVulkanWindow
    struct FrameResources {
        vk::Fence fence = nullptr;
        bool fence_waitable = false;
        vk::raii::Semaphore image_semaphore_raii = nullptr;
        vk::raii::Semaphore draw_semaphore_raii = nullptr;
        vk::raii::Semaphore present_semaphore_raii = nullptr;
        bool image_acquired = false;
        bool image_semaphore_waitable = false;
    };

    vk::raii::Context _context;
    vk::raii::Instance _instance = nullptr;
    vk::raii::DebugUtilsMessengerEXT _debug_messenger = nullptr;
    vk::raii::PhysicalDevice _physical_device = nullptr;
    vk::raii::Device _device = nullptr;
    vk::raii::SurfaceKHR _surface = nullptr;


    uint32_t _graphics_queue_family_index;
    uint32_t _present_queue_family_index;
    vk::raii::Queue _graphics_queue_raii = nullptr;
    vk::raii::Queue _present_queue_raii = nullptr;

    size_t _current_swapchain_index;
    vk::Extent2D _swapchain_extent;
    vk::SurfaceFormatKHR _surface_format;
    std::vector<vk::raii::ImageView> _swapchain_image_views;
    std::vector<vk::raii::Framebuffer> _swapchain_framebuffers;
    vk::raii::SwapchainKHR _swapchain = nullptr;

    vk::raii::PipelineLayout _pipeline_layout = nullptr;
    vk::raii::Pipeline _pipeline = nullptr;
    vk::raii::RenderPass _default_render_pass = nullptr;

    vk::raii::CommandPool _graphics_command_pool = nullptr;

    std::vector<vk::raii::CommandBuffer> _command_buffers;

    vk::raii::Semaphore image_available_semaphore = nullptr;
    vk::raii::Semaphore render_finished_semaphore = nullptr;
    vk::raii::Fence in_flight_fence = nullptr;
};
#endif

