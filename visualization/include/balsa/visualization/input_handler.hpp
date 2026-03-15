#if !defined(BALSA_VISUALIZATION_INPUT_HANDLER_HPP)
#define BALSA_VISUALIZATION_INPUT_HANDLER_HPP

namespace balsa::visualization {

// Modifier key bitmask values (matches GLFW modifier bits)
enum Modifiers : int {
    ModShift = 0x0001,
    ModControl = 0x0002,
    ModAlt = 0x0004,
    ModSuper = 0x0008,
};

struct MouseEvent {
    enum class Type { Press,
                      Release,
                      Move,
                      Scroll };
    Type type;
    double x = 0;
    double y = 0;
    int button = -1;// 0=left, 1=right, 2=middle. -1 for move/scroll.
    double scroll_x = 0;
    double scroll_y = 0;
    int modifiers = 0;// bitmask of Modifiers
};

struct KeyEvent {
    enum class Type { Press,
                      Release,
                      Repeat };
    Type type;
    int key = 0;// key code (GLFW key codes as canonical set)
    int scancode = 0;
    int modifiers = 0;// bitmask of Modifiers
};

struct WindowResizeEvent {
    int width = 0;
    int height = 0;
    int framebuffer_width = 0;
    int framebuffer_height = 0;
};


// Interface for receiving input events from a window.
// Implement this and pass it to Window::set_input_handler().
// Default implementations are no-ops so you only override what you need.
class InputHandler {
  public:
    virtual ~InputHandler() = default;
    virtual void on_mouse_event(const MouseEvent &) {}
    virtual void on_key_event(const KeyEvent &) {}
    virtual void on_window_resize(const WindowResizeEvent &) {}
};

}// namespace balsa::visualization

#endif
