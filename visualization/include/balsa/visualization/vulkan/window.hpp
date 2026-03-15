#if !defined(BALSA_VISUALIZATION_VULKAN_WINDOW_HPP)
#define BALSA_VISUALIZATION_VULKAN_WINDOW_HPP

#include <memory>
#include <string_view>
#include <glm/vec2.hpp>
#include "balsa/visualization/vulkan/scene_base.hpp"
#include "balsa/visualization/input_handler.hpp"

namespace balsa::visualization::vulkan {
class Film;

// Unified abstract base class for Vulkan windows.
// Both GLFW and Qt backends derive from this.
class Window {
  public:
    Window();
    virtual ~Window();

    // === Lifecycle ===
    // Enter the main loop (blocking). Returns an exit code.
    virtual int exec() = 0;
    // Request the window to close. May not close immediately.
    virtual void request_close() = 0;

    // === Film access ===
    // Returns the Film for rendering. Valid after construction / initialization.
    virtual Film &film() = 0;

    // === Scene management ===
    void set_scene(std::shared_ptr<SceneBase> scene);
    const std::shared_ptr<SceneBase> &scene() const;

    // === Input ===
    void set_input_handler(std::shared_ptr<InputHandler> handler);
    const std::shared_ptr<InputHandler> &input_handler() const;

    // === Window properties ===
    virtual glm::uvec2 framebuffer_size() const = 0;
    virtual void set_title(std::string_view title) = 0;

  protected:
    // The standard draw protocol: pre_draw -> scene->draw -> post_draw.
    // Subclasses call this at the appropriate time in their event/render loop.
    void draw_frame();

    // Subclasses override to add backend-specific behavior.
    // Default: scene->begin_render_pass / end_render_pass.
    virtual void pre_draw_hook();
    virtual void post_draw_hook();

    // Dispatch helpers — subclasses call these to forward events to the InputHandler.
    void dispatch_mouse(const MouseEvent &e);
    void dispatch_key(const KeyEvent &e);
    void dispatch_resize(const WindowResizeEvent &e);

  private:
    std::shared_ptr<SceneBase> _scene;
    std::shared_ptr<InputHandler> _input_handler;
};
}// namespace balsa::visualization::vulkan

#endif
