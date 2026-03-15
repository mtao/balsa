#if !defined(BALSA_VISUALIZATION_VULKAN_IMGUI_INTEGRATION_HPP)
#define BALSA_VISUALIZATION_VULKAN_IMGUI_INTEGRATION_HPP

#include <vulkan/vulkan.h>

// Forward declarations -- avoid pulling in GLFW / ImGui headers
struct GLFWwindow;
struct ImGuiContext;

namespace balsa::visualization::vulkan {

class Film;

// Standalone ImGui integration for Vulkan rendering.
//
// This class owns the ImGui context, a VkDescriptorPool for ImGui's internal
// use, and manages the lifecycle of ImGui's Vulkan and (optionally) GLFW
// platform backends.
//
// Usage:
//   ImGuiIntegration imgui;
//   imgui.init(film, glfw_window);          // after Film::initialize()
//
//   // Inside your scene's draw(), between begin/end render pass:
//   imgui.new_frame();
//   ImGui::ShowDemoWindow();                // your widgets here
//   imgui.render(film);
//
//   // On shutdown (or destructor):
//   imgui.shutdown(film);
//
// Ownership:
//   - Owns: ImGuiContext, VkDescriptorPool
//   - Borrows: Film (for Vulkan state), GLFWwindow (for input)
//   - ImGui internally owns: pipeline, shaders, font texture, per-frame buffers
//
// The Film must outlive this object. Call shutdown() before Film is destroyed,
// or let the destructor handle it (which requires the Film's device to still
// be valid).
class ImGuiIntegration {
  public:
    ImGuiIntegration() = default;
    ~ImGuiIntegration();

    // Non-copyable (owns Vulkan resources)
    ImGuiIntegration(const ImGuiIntegration &) = delete;
    ImGuiIntegration &operator=(const ImGuiIntegration &) = delete;

    // Movable
    ImGuiIntegration(ImGuiIntegration &&other) noexcept;
    ImGuiIntegration &operator=(ImGuiIntegration &&other) noexcept;

    // Initialize ImGui with Vulkan rendering backend.
    // If glfw_window is non-null, also initializes the GLFW platform backend
    // (input handling, clipboard, cursor).
    // Must be called after Film::initialize() / the render pass exists.
    void init(Film &film, GLFWwindow *glfw_window = nullptr);

    // Begin a new ImGui frame. Call once per frame before any ImGui widget calls.
    void new_frame();

    // Finalize and record ImGui draw commands into the current command buffer.
    // Must be called inside an active render pass (after your scene geometry,
    // so ImGui renders on top).
    void render(Film &film);

    // Tear down ImGui backends and free Vulkan resources.
    // Safe to call multiple times; subsequent calls are no-ops.
    // If not called explicitly, the destructor will attempt cleanup.
    void shutdown();

    // Query whether init() has been called (and shutdown() has not).
    bool is_initialized() const { return _context != nullptr; }

  private:
    // Upload font atlas to GPU via a one-shot command buffer.
    void upload_fonts(Film &film);

    ImGuiContext *_context = nullptr;
    VkDescriptorPool _descriptor_pool = VK_NULL_HANDLE;
    VkDevice _device = VK_NULL_HANDLE;// cached for cleanup
    bool _has_glfw_backend = false;
};

}// namespace balsa::visualization::vulkan

#endif
