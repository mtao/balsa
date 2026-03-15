#include "balsa/visualization/glfw/vulkan/window.hpp"
#include "balsa/visualization/glfw/vulkan/film.hpp"
#include "balsa/visualization/input_handler.hpp"
#include <GLFW/glfw3.h>
#include <spdlog/spdlog.h>
#include <stdexcept>

namespace balsa::visualization::glfw::vulkan {

// ============================================================
// Helper: retrieve our Window* from GLFW's user pointer.
// ============================================================
static Window *self(GLFWwindow *w) {
    return static_cast<Window *>(glfwGetWindowUserPointer(w));
}

// Helper: translate GLFW modifier bits to our Modifiers bitmask.
// They happen to match today, but an explicit mapping is future-proof.
static int translate_mods(int glfw_mods) {
    int m = 0;
    if (glfw_mods & GLFW_MOD_SHIFT) m |= ModShift;
    if (glfw_mods & GLFW_MOD_CONTROL) m |= ModControl;
    if (glfw_mods & GLFW_MOD_ALT) m |= ModAlt;
    if (glfw_mods & GLFW_MOD_SUPER) m |= ModSuper;
    return m;
}

// ============================================================
// Construction / Destruction
// ============================================================

Window::Window(const std::string_view &title, int width, int height)
  : visualization::vulkan::Window(), _glfw_window(nullptr), _film(nullptr), _title(title) {

    // Prevent GLFW from creating an OpenGL context.
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    _glfw_window = glfwCreateWindow(width, height, _title.c_str(), nullptr, nullptr);

    const char *description = nullptr;
    int code = glfwGetError(&description);
    if (code != GLFW_NO_ERROR) {
        spdlog::error("GLFW window creation failed: {}", description ? description : "unknown");
        throw std::runtime_error("Could not create GLFW window");
    }

    // Register ourselves as the user pointer so static callbacks can find us.
    glfwSetWindowUserPointer(_glfw_window, this);
    register_callbacks();

    // Now that we have a window, construct the Film (which triggers Vulkan init).
    _film = Film(_glfw_window);
}

Window::~Window() {
    // The scene (held by vulkan::Window base class) may own Vulkan objects
    // (pipelines, etc.) that require the device to still be alive during
    // destruction.  Release it here — before we tear down the Film —
    // so its destructor runs while the device is valid.
    vk::Device dev = _film.device();
    if (dev) {
        dev.waitIdle();
    }
    set_scene(nullptr);

    // Film must be destroyed before the GLFW window (it references the surface).
    // Move-assign a null Film to trigger destruction of the old one.
    _film = Film(nullptr);

    if (_glfw_window) {
        glfwSetWindowUserPointer(_glfw_window, nullptr);
        glfwDestroyWindow(_glfw_window);
        _glfw_window = nullptr;
    }
}

// ============================================================
// vulkan::Window interface
// ============================================================

int Window::exec() {
    const auto &sc = scene();
    if (sc) {
        sc->initialize(film());
    }

    while (!glfwWindowShouldClose(_glfw_window)) {
        // When the window is minimized (iconified) the framebuffer has
        // zero extent.  Vulkan cannot create a swapchain with 0×0 images,
        // so we must skip drawing entirely and wait until the user restores
        // the window.
        int fb_w = 0, fb_h = 0;
        glfwGetFramebufferSize(_glfw_window, &fb_w, &fb_h);
        if (fb_w == 0 || fb_h == 0) {
            glfwWaitEvents();
            continue;
        }

        draw_frame();
    }

    // Make sure the device is idle before we tear down.
    film().device().waitIdle();
    return 0;
}

void Window::request_close() {
    glfwSetWindowShouldClose(_glfw_window, GLFW_TRUE);
}

Film &Window::film() {
    return _film;
}

glm::uvec2 Window::framebuffer_size() const {
    int w = 0, h = 0;
    glfwGetFramebufferSize(_glfw_window, &w, &h);
    return glm::uvec2(static_cast<uint32_t>(w), static_cast<uint32_t>(h));
}

void Window::set_title(std::string_view title) {
    _title = title;
    glfwSetWindowTitle(_glfw_window, _title.c_str());
}

// ============================================================
// Draw hooks
// ============================================================

void Window::pre_draw_hook() {
    glfwPollEvents();
    _film.pre_draw_hook();
    // Base class begins the render pass (if a scene is set).
    visualization::vulkan::Window::pre_draw_hook();
}

void Window::post_draw_hook() {
    // Base class ends the render pass (if a scene is set).
    visualization::vulkan::Window::post_draw_hook();
    _film.post_draw_hook();
    _film.device().waitIdle();
}

// ============================================================
// Callback registration
// ============================================================

void Window::register_callbacks() {
    // Window callbacks
    glfwSetFramebufferSizeCallback(_glfw_window, cb_framebuffer_size);
    glfwSetWindowCloseCallback(_glfw_window, cb_window_close);
    glfwSetWindowRefreshCallback(_glfw_window, cb_window_refresh);
    glfwSetWindowFocusCallback(_glfw_window, cb_window_focus);
    glfwSetWindowIconifyCallback(_glfw_window, cb_window_iconify);

    // Mouse callbacks
    glfwSetMouseButtonCallback(_glfw_window, cb_mouse_button);
    glfwSetCursorPosCallback(_glfw_window, cb_cursor_pos);
    glfwSetScrollCallback(_glfw_window, cb_scroll);

    // Key callbacks
    glfwSetKeyCallback(_glfw_window, cb_key);
}

// ============================================================
// Static GLFW callbacks -- window
// ============================================================

void Window::cb_framebuffer_size(GLFWwindow *w, int width, int height) {
    auto *win = self(w);
    if (!win) return;

    spdlog::debug("Framebuffer resized to {}x{}", width, height);

    // Notify the InputHandler
    WindowResizeEvent e;
    int ww = 0, wh = 0;
    glfwGetWindowSize(w, &ww, &wh);
    e.width = ww;
    e.height = wh;
    e.framebuffer_width = width;
    e.framebuffer_height = height;
    win->dispatch_resize(e);
}

void Window::cb_window_close(GLFWwindow *w) {
    // GLFW already sets the close flag. Nothing extra needed.
    (void)w;
}

void Window::cb_window_refresh(GLFWwindow *w) {
    auto *win = self(w);
    if (!win) return;
    // Redraw immediately (e.g. during a live resize on some platforms).
    win->draw_frame();
}

void Window::cb_window_focus(GLFWwindow *w, int focused) {
    (void)w;
    (void)focused;
}

void Window::cb_window_iconify(GLFWwindow *w, int iconified) {
    (void)w;
    (void)iconified;
}

// ============================================================
// Static GLFW callbacks -- mouse
// ============================================================

void Window::cb_mouse_button(GLFWwindow *w, int button, int action, int mods) {
    auto *win = self(w);
    if (!win) return;

    MouseEvent e;
    e.type = (action == GLFW_PRESS) ? MouseEvent::Type::Press : MouseEvent::Type::Release;
    e.button = button;// GLFW: 0=left, 1=right, 2=middle — matches our convention
    e.modifiers = translate_mods(mods);

    // Fill in current cursor position
    glfwGetCursorPos(w, &e.x, &e.y);

    win->dispatch_mouse(e);
}

void Window::cb_cursor_pos(GLFWwindow *w, double xpos, double ypos) {
    auto *win = self(w);
    if (!win) return;

    MouseEvent e;
    e.type = MouseEvent::Type::Move;
    e.x = xpos;
    e.y = ypos;
    e.modifiers = 0;// GLFW doesn't pass mods for cursor pos; could query if needed

    win->dispatch_mouse(e);
}

void Window::cb_scroll(GLFWwindow *w, double xoffset, double yoffset) {
    auto *win = self(w);
    if (!win) return;

    MouseEvent e;
    e.type = MouseEvent::Type::Scroll;
    e.scroll_x = xoffset;
    e.scroll_y = yoffset;
    e.modifiers = 0;

    // Fill in current cursor position
    glfwGetCursorPos(w, &e.x, &e.y);

    win->dispatch_mouse(e);
}

// ============================================================
// Static GLFW callbacks -- keyboard
// ============================================================

void Window::cb_key(GLFWwindow *w, int key, int scancode, int action, int mods) {
    auto *win = self(w);
    if (!win) return;

    KeyEvent e;
    if (action == GLFW_PRESS) {
        e.type = KeyEvent::Type::Press;
    } else if (action == GLFW_RELEASE) {
        e.type = KeyEvent::Type::Release;
    } else {// GLFW_REPEAT
        e.type = KeyEvent::Type::Repeat;
    }
    e.key = key;
    e.scancode = scancode;
    e.modifiers = translate_mods(mods);

    win->dispatch_key(e);
}

}// namespace balsa::visualization::glfw::vulkan
