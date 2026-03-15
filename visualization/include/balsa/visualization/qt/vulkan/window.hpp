#if !defined(BALSA_VISUALIZATION_QT_VULKAN_WINDOW_HPP)
#define BALSA_VISUALIZATION_QT_VULKAN_WINDOW_HPP

#include <memory>
#include <string>
#include <QVulkanWindow>
#include "balsa/visualization/vulkan/window.hpp"
#include "balsa/visualization/qt/vulkan/film.hpp"

namespace balsa::visualization::qt::vulkan {

// Concrete Qt + Vulkan window.
// Multiple inheritance: QVulkanWindow (Qt windowing / Vulkan lifecycle)
//                     + vulkan::Window (unified scene/input/draw protocol).
//
// QVulkanWindow owns the Vulkan instance, device, swapchain, depth/stencil,
// and MSAA resources.  Our qt::vulkan::Film is a thin wrapper that delegates
// to QVulkanWindow's accessors.
//
// The inner Renderer class bridges Qt's push model (startNextFrame) to our
// pull model (draw_frame) by calling draw_frame() from startNextFrame().
class Window : public QVulkanWindow
  , public visualization::vulkan::Window {
  public:
    explicit Window(const std::string &title = "Balsa", int width = 600, int height = 400);
    ~Window() override;

    // Non-copyable, non-movable
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

  protected:
    // QVulkanWindow override: creates our inner Renderer.
    QVulkanWindowRenderer *createRenderer() override;

    // Qt window lifecycle
    void closeEvent(QCloseEvent *e) override;

    // Qt input event overrides (dispatch to InputHandler)
    void mousePressEvent(QMouseEvent *e) override;
    void mouseReleaseEvent(QMouseEvent *e) override;
    void mouseMoveEvent(QMouseEvent *e) override;
    void wheelEvent(QWheelEvent *e) override;
    void keyPressEvent(QKeyEvent *e) override;
    void keyReleaseEvent(QKeyEvent *e) override;
    void resizeEvent(QResizeEvent *e) override;

  private:
    std::string _title;
    int _initial_width;
    int _initial_height;
    bool _closing = false;

    // Persistent Film, created in Renderer::initResources() and valid
    // until Renderer::releaseResources().
    std::unique_ptr<Film> _film;

    // Helper: translate Qt key modifiers to our Modifiers bitmask.
    static int translate_mods(Qt::KeyboardModifiers mods);
    // Helper: translate Qt mouse button enum to our int convention (0=left,1=right,2=middle).
    static int translate_button(Qt::MouseButton button);

    // Inner renderer that bridges Qt's push model to our draw_frame().
    class Renderer;
    friend class Renderer;
};

}// namespace balsa::visualization::qt::vulkan

#endif
