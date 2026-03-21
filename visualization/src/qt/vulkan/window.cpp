#include "balsa/visualization/qt/vulkan/window.hpp"
#include "balsa/visualization/input_handler.hpp"
#include <spdlog/spdlog.h>
#include <QGuiApplication>
#include <QVulkanInstance>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QCloseEvent>

namespace balsa::visualization::qt::vulkan {

// ============================================================
// Inner Renderer: bridges Qt's push model to our draw_frame()
// ============================================================

class Window::Renderer : public QVulkanWindowRenderer {
  public:
    Renderer(Window *w) : _outer(w) {}

    void initResources() override {
        spdlog::debug("Qt Renderer: initResources");
        // Create the persistent Film now that QVulkanWindow has a device
        _outer->_film = std::make_unique<Film>(*static_cast<QVulkanWindow *>(_outer));

        // Initialize the scene if one is already set
        const auto &sc = _outer->scene();
        if (sc) {
            sc->initialize(_outer->film());
        }
    }

    void initSwapChainResources() override {
        spdlog::debug("Qt Renderer: initSwapChainResources");
    }

    void releaseSwapChainResources() override {
        spdlog::debug("Qt Renderer: releaseSwapChainResources");
    }

    void releaseResources() override {
        spdlog::debug("Qt Renderer: releaseResources");
        // Release scene's Vulkan resources while the device is still valid.
        // This handles the case where external shared_ptr holders keep the
        // scene alive beyond this point.
        const auto &sc = _outer->scene();
        if (sc) {
            sc->release_vulkan_resources();
        }
        _outer->set_scene(nullptr);
        _outer->_film.reset();
    }

    void startNextFrame() override {
        // Bridge: Qt says "render now" -> we call draw_frame() which
        // runs pre_draw_hook -> scene->draw -> post_draw_hook.
        if (_outer->_film && !_outer->_closing) {
            _outer->draw_frame();
        }

        // Signal Qt that the frame is ready for presentation.
        _outer->QVulkanWindow::frameReady();
        // Request continuous rendering (throttled by vsync/presentation rate),
        // but only if the window is not closing.
        if (!_outer->_closing) {
            _outer->QVulkanWindow::requestUpdate();
        }
    }

  private:
    Window *_outer;
};


// ============================================================
// Modifier / button translation helpers
// ============================================================

int Window::translate_mods(Qt::KeyboardModifiers mods) {
    int m = 0;
    if (mods & Qt::ShiftModifier) m |= ModShift;
    if (mods & Qt::ControlModifier) m |= ModControl;
    if (mods & Qt::AltModifier) m |= ModAlt;
    if (mods & Qt::MetaModifier) m |= ModSuper;
    return m;
}

int Window::translate_button(Qt::MouseButton button) {
    switch (button) {
    case Qt::LeftButton:
        return 0;
    case Qt::RightButton:
        return 1;
    case Qt::MiddleButton:
        return 2;
    default:
        return -1;
    }
}


// ============================================================
// Construction / Destruction
// ============================================================

Window::Window(const std::string &title, int width, int height)
  : QVulkanWindow(), visualization::vulkan::Window(), _title(title), _initial_width(width), _initial_height(height) {
}

Window::~Window() {
    // Film is owned by unique_ptr and will be destroyed automatically.
    // QVulkanWindow handles its own cleanup.
}


// ============================================================
// vulkan::Window interface
// ============================================================

int Window::exec() {
    // The caller must have set a QVulkanInstance before calling exec().
    // Creating one as a local variable here would cause a use-after-free:
    // QGuiApplication::exec() returns, the local is destroyed, but
    // QVulkanWindow still holds a dangling pointer to it.
    if (!QVulkanWindow::vulkanInstance()) {
        spdlog::error("No QVulkanInstance set on Window — call setVulkanInstance() before exec()");
        return 1;
    }

    QVulkanWindow::setTitle(QString::fromStdString(_title));
    QVulkanWindow::resize(_initial_width, _initial_height);
    QVulkanWindow::show();

    return QGuiApplication::exec();
}

void Window::request_close() {
    _closing = true;
    QVulkanWindow::close();
}

Film &Window::film() {
    if (!_film) {
        throw std::runtime_error("Qt Film not yet initialized (before initResources)");
    }
    return *_film;
}

std::array<uint32_t, 2> Window::framebuffer_size() const {
    const QSize sz = QVulkanWindow::size() * QVulkanWindow::devicePixelRatio();
    return { static_cast<uint32_t>(sz.width()),
             static_cast<uint32_t>(sz.height()) };
}

void Window::set_title(std::string_view title) {
    _title = title;
    QVulkanWindow::setTitle(QString::fromUtf8(_title.data(), static_cast<int>(_title.size())));
}

QVulkanWindowRenderer *Window::createRenderer() {
    return new Renderer(this);
}


// ============================================================
// Qt window lifecycle
// ============================================================

void Window::closeEvent(QCloseEvent *e) {
    _closing = true;
    QVulkanWindow::closeEvent(e);
    QGuiApplication::quit();
}


// ============================================================
// Qt input event overrides
// ============================================================

void Window::mousePressEvent(QMouseEvent *e) {
    MouseEvent me;
    me.type = MouseEvent::Type::Press;
    me.x = e->position().x();
    me.y = e->position().y();
    me.button = translate_button(e->button());
    me.modifiers = translate_mods(e->modifiers());
    dispatch_mouse(me);
}

void Window::mouseReleaseEvent(QMouseEvent *e) {
    MouseEvent me;
    me.type = MouseEvent::Type::Release;
    me.x = e->position().x();
    me.y = e->position().y();
    me.button = translate_button(e->button());
    me.modifiers = translate_mods(e->modifiers());
    dispatch_mouse(me);
}

void Window::mouseMoveEvent(QMouseEvent *e) {
    MouseEvent me;
    me.type = MouseEvent::Type::Move;
    me.x = e->position().x();
    me.y = e->position().y();
    me.button = -1;
    me.modifiers = translate_mods(e->modifiers());
    dispatch_mouse(me);
}

void Window::wheelEvent(QWheelEvent *e) {
    MouseEvent me;
    me.type = MouseEvent::Type::Scroll;
    me.x = e->position().x();
    me.y = e->position().y();
    me.button = -1;
    // Qt provides scroll in 1/8 degree increments; normalize to "lines"
    // (most mice send 15-degree steps = 120 units per line)
    me.scroll_x = e->angleDelta().x() / 120.0;
    me.scroll_y = e->angleDelta().y() / 120.0;
    me.modifiers = translate_mods(e->modifiers());
    dispatch_mouse(me);
}

void Window::keyPressEvent(QKeyEvent *e) {
    KeyEvent ke;
    ke.type = e->isAutoRepeat() ? KeyEvent::Type::Repeat : KeyEvent::Type::Press;
    ke.key = e->key();
    ke.scancode = e->nativeScanCode();
    ke.modifiers = translate_mods(e->modifiers());
    dispatch_key(ke);
}

void Window::keyReleaseEvent(QKeyEvent *e) {
    KeyEvent ke;
    ke.type = KeyEvent::Type::Release;
    ke.key = e->key();
    ke.scancode = e->nativeScanCode();
    ke.modifiers = translate_mods(e->modifiers());
    dispatch_key(ke);
}

void Window::resizeEvent(QResizeEvent *e) {
    QVulkanWindow::resizeEvent(e);// let Qt handle its part first

    WindowResizeEvent we;
    we.width = e->size().width();
    we.height = e->size().height();
    qreal dpr = QVulkanWindow::devicePixelRatio();
    we.framebuffer_width = static_cast<int>(e->size().width() * dpr);
    we.framebuffer_height = static_cast<int>(e->size().height() * dpr);
    dispatch_resize(we);
}

}// namespace balsa::visualization::qt::vulkan
