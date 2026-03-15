#include "balsa/visualization/vulkan/imgui_integration.hpp"
#include "balsa/visualization/vulkan/film.hpp"

#include <imgui.h>
#include <imgui_impl_vulkan.h>
#include <imgui_impl_glfw.h>

#include <spdlog/spdlog.h>
#include <stdexcept>
#include <cassert>

namespace balsa::visualization::vulkan {

// ---------------------------------------------------------------------------
// Move semantics
// ---------------------------------------------------------------------------

ImGuiIntegration::ImGuiIntegration(ImGuiIntegration &&other) noexcept
  : _context(other._context),
    _descriptor_pool(other._descriptor_pool),
    _device(other._device),
    _has_glfw_backend(other._has_glfw_backend) {
    other._context = nullptr;
    other._descriptor_pool = VK_NULL_HANDLE;
    other._device = VK_NULL_HANDLE;
    other._has_glfw_backend = false;
}

ImGuiIntegration &ImGuiIntegration::operator=(ImGuiIntegration &&other) noexcept {
    if (this != &other) {
        shutdown();
        _context = other._context;
        _descriptor_pool = other._descriptor_pool;
        _device = other._device;
        _has_glfw_backend = other._has_glfw_backend;
        other._context = nullptr;
        other._descriptor_pool = VK_NULL_HANDLE;
        other._device = VK_NULL_HANDLE;
        other._has_glfw_backend = false;
    }
    return *this;
}

// ---------------------------------------------------------------------------
// Destructor
// ---------------------------------------------------------------------------

ImGuiIntegration::~ImGuiIntegration() {
    if (is_initialized()) {
        // Best-effort cleanup. In normal use, the caller should have called
        // shutdown() explicitly before the Film's device is destroyed.
        shutdown();
    }
}

// ---------------------------------------------------------------------------
// init
// ---------------------------------------------------------------------------

void ImGuiIntegration::init(Film &film, GLFWwindow *glfw_window) {
    if (is_initialized()) {
        spdlog::warn("ImGuiIntegration::init() called but already initialized — ignoring");
        return;
    }

    // --- Create ImGui context ---
    _context = ImGui::CreateContext();
    ImGui::SetCurrentContext(_context);

    ImGuiIO &io = ImGui::GetIO();
    // Enable keyboard nav (optional, but nice default)
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    // --- Create descriptor pool for ImGui ---
    // ImGui needs a pool for its font texture descriptor (and any user textures).
    // A single COMBINED_IMAGE_SAMPLER of size 1000 is generous and conventional.
    VkDescriptorPoolSize pool_size = {};
    pool_size.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    pool_size.descriptorCount = 1000;

    VkDescriptorPoolCreateInfo pool_info = {};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    pool_info.maxSets = 1000;
    pool_info.poolSizeCount = 1;
    pool_info.pPoolSizes = &pool_size;

    _device = static_cast<VkDevice>(film.device());
    VkResult result = vkCreateDescriptorPool(_device, &pool_info, nullptr, &_descriptor_pool);
    if (result != VK_SUCCESS) {
        ImGui::DestroyContext(_context);
        _context = nullptr;
        throw std::runtime_error("ImGuiIntegration: failed to create descriptor pool");
    }

    // --- Populate ImGui Vulkan init info ---
    ImGui_ImplVulkan_InitInfo init_info = {};
    init_info.Instance = static_cast<VkInstance>(film.instance());
    init_info.PhysicalDevice = static_cast<VkPhysicalDevice>(film.physical_device());
    init_info.Device = _device;
    init_info.QueueFamily = film.graphics_queue_family_index();
    init_info.Queue = static_cast<VkQueue>(film.graphics_queue());
    init_info.PipelineCache = VK_NULL_HANDLE;
    init_info.DescriptorPool = _descriptor_pool;
    init_info.Subpass = 0;
    init_info.MinImageCount = film.swapchain_image_count();
    init_info.ImageCount = film.swapchain_image_count();
    init_info.MSAASamples = static_cast<VkSampleCountFlagBits>(film.sample_count());
    init_info.Allocator = nullptr;
    init_info.CheckVkResultFn = [](VkResult err) {
        if (err != VK_SUCCESS) {
            spdlog::error("ImGui Vulkan error: {}", static_cast<int>(err));
        }
    };

    // --- Init Vulkan rendering backend ---
    if (!ImGui_ImplVulkan_Init(&init_info, static_cast<VkRenderPass>(film.default_render_pass()))) {
        vkDestroyDescriptorPool(_device, _descriptor_pool, nullptr);
        _descriptor_pool = VK_NULL_HANDLE;
        ImGui::DestroyContext(_context);
        _context = nullptr;
        throw std::runtime_error("ImGuiIntegration: ImGui_ImplVulkan_Init failed");
    }

    // --- Init GLFW platform backend (optional) ---
    if (glfw_window) {
        // install_callbacks = true: ImGui chains onto existing GLFW callbacks
        ImGui_ImplGlfw_InitForVulkan(glfw_window, true);
        _has_glfw_backend = true;
    }

    // --- Upload fonts ---
    upload_fonts(film);

    spdlog::debug("ImGuiIntegration initialized (GLFW backend: {})", _has_glfw_backend);
}

// ---------------------------------------------------------------------------
// upload_fonts
// ---------------------------------------------------------------------------

void ImGuiIntegration::upload_fonts(Film &film) {
    // One-shot command buffer to upload the font atlas to the GPU.
    VkCommandPool cmd_pool = static_cast<VkCommandPool>(film.graphics_command_pool());

    VkCommandBufferAllocateInfo alloc_info = {};
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.commandPool = cmd_pool;
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandBufferCount = 1;

    VkCommandBuffer cmd_buf;
    vkAllocateCommandBuffers(_device, &alloc_info, &cmd_buf);

    VkCommandBufferBeginInfo begin_info = {};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(cmd_buf, &begin_info);
    ImGui_ImplVulkan_CreateFontsTexture(cmd_buf);
    vkEndCommandBuffer(cmd_buf);

    VkSubmitInfo submit_info = {};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &cmd_buf;

    VkQueue queue = static_cast<VkQueue>(film.graphics_queue());
    vkQueueSubmit(queue, 1, &submit_info, VK_NULL_HANDLE);
    vkQueueWaitIdle(queue);

    vkFreeCommandBuffers(_device, cmd_pool, 1, &cmd_buf);

    // ImGui internally allocated staging buffers for the upload — clean them up
    ImGui_ImplVulkan_DestroyFontUploadObjects();
}

// ---------------------------------------------------------------------------
// new_frame
// ---------------------------------------------------------------------------

void ImGuiIntegration::new_frame() {
    assert(is_initialized() && "ImGuiIntegration::new_frame() called before init()");
    ImGui::SetCurrentContext(_context);
    ImGui_ImplVulkan_NewFrame();
    if (_has_glfw_backend) {
        ImGui_ImplGlfw_NewFrame();
    }
    ImGui::NewFrame();
}

// ---------------------------------------------------------------------------
// render
// ---------------------------------------------------------------------------

void ImGuiIntegration::render(Film &film) {
    assert(is_initialized() && "ImGuiIntegration::render() called before init()");
    ImGui::SetCurrentContext(_context);
    ImGui::Render();
    ImGui_ImplVulkan_RenderDrawData(
      ImGui::GetDrawData(),
      static_cast<VkCommandBuffer>(film.current_command_buffer()));
}

// ---------------------------------------------------------------------------
// shutdown
// ---------------------------------------------------------------------------

void ImGuiIntegration::shutdown() {
    if (!is_initialized()) {
        return;
    }

    ImGui::SetCurrentContext(_context);

    // Wait for the device to finish all pending work before tearing down
    if (_device != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(_device);
    }

    ImGui_ImplVulkan_Shutdown();

    if (_has_glfw_backend) {
        ImGui_ImplGlfw_Shutdown();
        _has_glfw_backend = false;
    }

    if (_descriptor_pool != VK_NULL_HANDLE && _device != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(_device, _descriptor_pool, nullptr);
        _descriptor_pool = VK_NULL_HANDLE;
    }

    ImGui::DestroyContext(_context);
    _context = nullptr;
    _device = VK_NULL_HANDLE;

    spdlog::debug("ImGuiIntegration shut down");
}

}// namespace balsa::visualization::vulkan
