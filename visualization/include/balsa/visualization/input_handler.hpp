#pragma once
#include <vector>

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
    int buttons = -1;// held buttons after the event (bit 1<<button), -1 if unknown
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

struct TouchPoint {
    enum class State { Pressed,
                       Moved,
                       Stationary,
                       Released };
    int id = -1;// stable for the lifetime of the contact
    double x = 0;// logical window coordinates, like MouseEvent
    double y = 0;
    State state = State::Stationary;
};

// A touchscreen event carrying every current contact, including ones that
// were just released.  Cancel means the platform aborted the sequence.
struct TouchEvent {
    enum class Type { Begin,
                      Update,
                      End,
                      Cancel };
    Type type;
    std::vector<TouchPoint> points;
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
    virtual void on_touch_event(const TouchEvent &) {}
    virtual void on_window_resize(const WindowResizeEvent &) {}
    // The window lost input focus; releases may never arrive, so drop any
    // in-progress drag or touch state.
    virtual void on_focus_lost() {}
};

}// namespace balsa::visualization
