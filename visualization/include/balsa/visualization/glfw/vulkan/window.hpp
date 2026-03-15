#if !defined(BALSA_VISUALIZATION_GLFW_VULKAN_WINDOW_HPP)
#define BALSA_VISUALIZATION_GLFW_VULKAN_WINDOW_HPP

#include <string>
#include <string_view>
#include <glm/vec2.hpp>
#include "balsa/visualization/vulkan/window.hpp"
#include "balsa/visualization/glfw/vulkan/film.hpp"

class GLFWwindow;

namespace balsa::visualization::glfw::vulkan {

// Concrete GLFW + Vulkan window.
// Single inheritance from vulkan::Window (the unified base).
// Owns both the GLFWwindow and the GLFW-specific Film.
class Window : public visualization::vulkan::Window {
  public:
    Window(const std::string_view &title, int width = 600, int height = 400);
    ~Window() override;

    // Non-copyable, non-movable (due to GLFW user-pointer registration)
    Window(const Window &) = delete;
    Window &operator=(const Window &) = delete;
    Window(Window &&) = delete;
    Window &operator=(Window &&) = delete;

    // === vulkan::Window interface ===
    int exec() override;
    void request_close() override;
    Film &film() override;
    glm::uvec2 framebuffer_size() const override;
    void set_title(std::string_view title) override;

    // Access the raw GLFW handle (e.g. for ImGui backends)
    GLFWwindow *glfw_window() const { return _glfw_window; }

  protected:
    void pre_draw_hook() override;
    void post_draw_hook() override;

  private:
    GLFWwindow *_glfw_window = nullptr;
    Film _film;
    std::string _title;

    // GLFW callback setup
    void register_callbacks();

    // --- Static GLFW callbacks (dispatch via user pointer) ---
    // Window callbacks
    static void cb_framebuffer_size(GLFWwindow *w, int width, int height);
    static void cb_window_close(GLFWwindow *w);
    static void cb_window_refresh(GLFWwindow *w);
    static void cb_window_focus(GLFWwindow *w, int focused);
    static void cb_window_iconify(GLFWwindow *w, int iconified);

    // Mouse callbacks
    static void cb_mouse_button(GLFWwindow *w, int button, int action, int mods);
    static void cb_cursor_pos(GLFWwindow *w, double xpos, double ypos);
    static void cb_scroll(GLFWwindow *w, double xoffset, double yoffset);

    // Key callbacks
    static void cb_key(GLFWwindow *w, int key, int scancode, int action, int mods);
};

}// namespace balsa::visualization::glfw::vulkan

#endif
