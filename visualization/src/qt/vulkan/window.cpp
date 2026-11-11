#include "balsa/visualization/qt/vulkan/window.hpp"
#include "balsa/visualization/input_handler.hpp"
#include "balsa/visualization/vulkan/logging.hpp"
#include <QCloseEvent>
#include <QFocusEvent>
#include <QGuiApplication>
#include <QMouseEvent>
#include <QNativeGestureEvent>
#include <QResizeEvent>
#include <QTouchEvent>
#include <QVulkanInstance>
#include <QWidget>
#include <QtMath>
#include <spdlog/spdlog.h>

namespace balsa::visualization::qt::vulkan {

// ============================================================
// Inner Renderer: bridges Qt's push model to our draw_frame()
// ============================================================

class Window::Renderer : public QVulkanWindowRenderer {
  public:
    Renderer(Window *w) : _outer(w) {}

    void initResources() override {
        ::balsa::visualization::vulkan::logger()->debug(
            "Qt Renderer: initResources");
        // Create the persistent Film now that QVulkanWindow has a device
        _outer->_film = std::make_unique<Film>(*static_cast<QVulkanWindow *>(_outer));

        // Initialize the scene if one is already set
        const auto &sc = _outer->scene();
        if (sc) {
            sc->initialize(_outer->film());
        }
    }

    void initSwapChainResources() override {
        ::balsa::visualization::vulkan::logger()->debug(
            "Qt Renderer: initSwapChainResources");
    }

    void releaseSwapChainResources() override {
        ::balsa::visualization::vulkan::logger()->debug(
            "Qt Renderer: releaseSwapChainResources");
    }

    void releaseResources() override {
        ::balsa::visualization::vulkan::logger()->debug(
            "Qt Renderer: releaseResources");
        // Release scene's Vulkan resources while the device is still valid
        // (external shared_ptr holders may keep the scene alive beyond
        // this point).  The scene itself stays: if Qt recreates the device
        // (e.g. after device loss), initResources() initializes it again.
        const auto &sc = _outer->scene();
        if (sc) {
            sc->release_vulkan_resources();
        }
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

int Window::translate_buttons(Qt::MouseButtons buttons) {
    int mask = 0;
    for (auto b : { Qt::LeftButton, Qt::RightButton, Qt::MiddleButton }) {
        if (buttons & b) mask |= 1 << translate_button(b);
    }
    return mask;
}

bool Window::is_from_touch(const QMouseEvent *e) {
    const auto *dev = e->pointingDevice();
    return dev && dev->type() == QInputDevice::DeviceType::TouchScreen;
}

// ============================================================
// Construction / Destruction
// ============================================================

Window::Window(const std::string &title, int width, int height)
  : QVulkanWindow(), visualization::vulkan::Window(), _title(title), _initial_width(width), _initial_height(height) {
    // Without this, QVulkanWindow releases every resource whenever the
    // window stops being exposed, which Wayland compositors report when
    // the window is on another workspace.
    setFlags(QVulkanWindow::PersistentResources);
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
        ::balsa::visualization::vulkan::logger()->error(
            "No QVulkanInstance set on Window — call setVulkanInstance() "
            "before exec()");
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
    if (is_from_touch(e)) return;
    MouseEvent me;
    me.type = MouseEvent::Type::Press;
    me.x = e->position().x();
    me.y = e->position().y();
    me.button = translate_button(e->button());
    me.buttons = translate_buttons(e->buttons());
    me.modifiers = translate_mods(e->modifiers());
    dispatch_mouse(me);
}

void Window::mouseReleaseEvent(QMouseEvent *e) {
    if (is_from_touch(e)) return;
    MouseEvent me;
    me.type = MouseEvent::Type::Release;
    me.x = e->position().x();
    me.y = e->position().y();
    me.button = translate_button(e->button());
    me.buttons = translate_buttons(e->buttons());
    me.modifiers = translate_mods(e->modifiers());
    dispatch_mouse(me);
}

void Window::mouseMoveEvent(QMouseEvent *e) {
    if (is_from_touch(e)) return;
    MouseEvent me;
    me.type = MouseEvent::Type::Move;
    me.x = e->position().x();
    me.y = e->position().y();
    me.button = -1;
    me.buttons = translate_buttons(e->buttons());
    me.modifiers = translate_mods(e->modifiers());
    dispatch_mouse(me);
}

void Window::wheelEvent(QWheelEvent *e) {
    // Touchpads report precise pixel deltas; mice report notches.
    const auto *dev = e->pointingDevice();
    const bool touchpad = (dev && dev->type() == QInputDevice::DeviceType::TouchPad)
                          || !e->pixelDelta().isNull();
    if (touchpad) {
        TouchpadEvent te;
        te.type = TouchpadEvent::Type::Scroll;
        te.x = e->position().x();
        te.y = e->position().y();
        const QPointF delta = e->pixelDelta().isNull()
                                ? QPointF(e->angleDelta()) / 8.0
                                : QPointF(e->pixelDelta());
        te.dx = delta.x();
        te.dy = delta.y();
        te.modifiers = translate_mods(e->modifiers());
        dispatch_touchpad(te);
        return;
    }
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

bool Window::handle_touch(QTouchEvent *e) {
    const auto *dev = e->pointingDevice();
    if (!dev || dev->type() != QInputDevice::DeviceType::TouchScreen) {
        return false;
    }

    TouchEvent te;
    switch (e->type()) {
    case QEvent::TouchBegin:
        te.type = TouchEvent::Type::Begin;
        break;
    case QEvent::TouchEnd:
        te.type = TouchEvent::Type::End;
        break;
    case QEvent::TouchCancel:
        te.type = TouchEvent::Type::Cancel;
        break;
    default:
        te.type = TouchEvent::Type::Update;
        break;
    }
    te.modifiers = translate_mods(e->modifiers());
    te.points.reserve(e->points().size());
    for (const auto &p : e->points()) {
        TouchPoint tp;
        tp.id = p.id();
        tp.x = p.position().x();
        tp.y = p.position().y();
        switch (p.state()) {
        case QEventPoint::State::Pressed:
            tp.state = TouchPoint::State::Pressed;
            break;
        case QEventPoint::State::Updated:
            tp.state = TouchPoint::State::Moved;
            break;
        case QEventPoint::State::Released:
            tp.state = TouchPoint::State::Released;
            break;
        default:
            tp.state = TouchPoint::State::Stationary;
            break;
        }
        te.points.push_back(tp);
    }
    dispatch_touch(te);
    e->accept();
    return true;
}

void Window::touchEvent(QTouchEvent *e) {
    if (!handle_touch(e)) e->ignore();
}

bool Window::handle_native_gesture(QNativeGestureEvent *e) {
    TouchpadEvent te;
    te.x = e->position().x();
    te.y = e->position().y();
    te.modifiers = translate_mods(e->modifiers());
    switch (e->gestureType()) {
    case Qt::ZoomNativeGesture:
        te.type = TouchpadEvent::Type::Pinch;
        te.scale = 1.0 + e->value();
        break;
    case Qt::RotateNativeGesture:
        // Qt reports degrees, clockwise-positive.
        te.type = TouchpadEvent::Type::Rotate;
        te.angle = qDegreesToRadians(e->value());
        break;
    case Qt::PanNativeGesture:
        te.type = TouchpadEvent::Type::Swipe;
        te.dx = e->delta().x();
        te.dy = e->delta().y();
        break;
    case Qt::BeginNativeGesture:
    case Qt::EndNativeGesture:
        e->accept();
        return true;
    default:
        return false;
    }
    dispatch_touchpad(te);
    e->accept();
    return true;
}

bool Window::event(QEvent *e) {
    if (e->type() == QEvent::NativeGesture
        && handle_native_gesture(static_cast<QNativeGestureEvent *>(e))) {
        return true;
    }
    return QVulkanWindow::event(e);
}

auto Window::create_container(QWidget *parent) -> QWidget * {
    auto *container = QWidget::createWindowContainer(this, parent);
    container->setAttribute(Qt::WA_AcceptTouchEvents);
    container->installEventFilter(this);
    return container;
}

bool Window::eventFilter(QObject *watched, QEvent *e) {
    switch (e->type()) {
    case QEvent::TouchBegin:
    case QEvent::TouchUpdate:
    case QEvent::TouchEnd:
    case QEvent::TouchCancel:
        return handle_touch(static_cast<QTouchEvent *>(e));
    case QEvent::NativeGesture:
        return handle_native_gesture(static_cast<QNativeGestureEvent *>(e));
    default:
        return QVulkanWindow::eventFilter(watched, e);
    }
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

void Window::focusOutEvent(QFocusEvent *e) {
    QVulkanWindow::focusOutEvent(e);
    dispatch_focus_lost();
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
