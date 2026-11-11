#pragma once
#include <utility>

#include "balsa/scene_graph/types.hpp"
#include <balsa/geometry/BoundingBox.hpp>
#include "balsa/visualization/input_handler.hpp"
#include "balsa/visualization/touch_gesture_tracker.hpp"

namespace balsa::visualization::vulkan {

class MeshScene;

// ── OrbitCameraController ───────────────────────────────────────────
//
// InputHandler that implements an orbit camera (arcball-style):
//
//   Left-drag   → orbit (rotate around the center point)
//   Right-drag  → pan (translate center + eye in the view plane)
//   Scroll      → zoom (move eye closer/further from center)
//   Touch: one finger orbits, two fingers pan, pinch zooms, twist rolls
//   Touchpad: two-finger scroll orbits (Shift pans, Ctrl zooms), pinch zooms
//
// Updates the MeshScene's view matrix via look_at() each time the
// camera state changes.
//
// The camera state is parameterized as spherical coordinates around
// a center point:
//   - theta: azimuthal angle (horizontal orbit)
//   - phi:   polar angle (vertical orbit, clamped to avoid gimbal lock)
//   - distance: eye-to-center distance
//   - center: the look-at target point
//
// Usage:
//   auto controller = std::make_shared<OrbitCameraController>(scene);
//   window.set_input_handler(controller);

class OrbitCameraController : public InputHandler {
  public:
    explicit OrbitCameraController(MeshScene *scene);

    // ── Configuration ───────────────────────────────────────────────

    auto set_distance(float d) -> void;
    auto set_center(const scene_graph::Vec3f &c) -> void;
    auto set_angles(float theta, float phi) -> void;
    // Rotation of the view about its viewing direction (radians; positive
    // turns the content clockwise on screen).  frame() resets it.
    auto set_roll(float roll) -> void;

    // Switches to the default camera, centres it on `box` and backs off
    // until the box fits the field of view.  The near and far planes follow
    // the box's size.  Empty boxes are ignored.
    auto frame(const geometry::BoundingBox<float, 3> &box) -> void;

    auto distance() const -> float { return _distance; }
    auto center() const -> const scene_graph::Vec3f & { return _center; }
    auto theta() const -> float { return _theta; }
    auto phi() const -> float { return _phi; }
    auto roll() const -> float { return _roll; }

    // Sensitivity parameters
    float orbit_sensitivity = 0.005f;
    // Pan speed per pixel (scaled by distance) used only until the
    // viewport size is known; afterwards panning tracks the cursor exactly
    // on the plane through the center.
    float pan_sensitivity = 0.003f;
    float zoom_sensitivity = 0.1f;
    // Multiplier on orbit_sensitivity for one-finger drags.
    double touch_orbit_scale = 1.0;
    // Multiplier on orbit_sensitivity for two-finger touchpad scrolls;
    // negate to flip the direction.
    double touchpad_orbit_scale = 1.0;
    // Zoom factor per pixel of Ctrl + two-finger scroll.
    double touchpad_zoom_sensitivity = 0.01;
    // Whether touchpad twists roll the view.
    bool touchpad_roll = true;
    // Multiplier on twist angles from touchpads; negate to flip.
    double touchpad_roll_scale = 1.0;

    // ── InputHandler overrides ──────────────────────────────────────

    auto on_mouse_event(const MouseEvent &e) -> void override;
    // Touchscreen:
    //   One-finger drag      → orbit
    //   Two-finger drag      → pan
    //   Pinch                → zoom
    auto on_touch_event(const TouchEvent &e) -> void override;
    // Touchpad:
    //   Two-finger scroll         → orbit
    //   Shift + two-finger scroll → pan
    //   Ctrl + two-finger scroll  → zoom toward the cursor
    //   Pinch                     → zoom toward the cursor
    //   Twist                     → roll about the cursor (if touchpad_roll)
    //   Swipe                     → pan
    auto on_touchpad_event(const TouchpadEvent &e) -> void override;
    auto on_window_resize(const WindowResizeEvent &e) -> void override;
    auto on_focus_lost() -> void override;

    // ── Camera motions (screen-space deltas in logical pixels) ──────

    auto orbit(double dx, double dy) -> void;
    // Moves the center in the view plane so the scene follows the cursor.
    auto pan(double dx, double dy) -> void;
    // factor > 1 moves the eye closer (distance /= factor).
    auto zoom(float factor) -> void;
    // Zooms while keeping the point under (x, y) fixed on screen
    // (exact on the plane through the center).
    auto zoom_at(float factor, double x, double y) -> void;

    // Rolls by angle (clockwise on screen) keeping the point under (x, y)
    // fixed (exact on the plane through the center).
    auto roll_at(double angle, double x, double y) -> void;

    // Logical window size; set automatically by on_window_resize.
    auto set_viewport_size(double width, double height) -> void;

  private:
    MeshScene *_scene = nullptr;

    // Spherical camera parameters
    float _theta = 0.0f;// azimuthal angle (radians)
    float _phi = 0.3f;// polar angle (radians, 0 = equator)
    float _distance = 3.0f;
    float _roll = 0.0f;
    scene_graph::Vec3f _center{ 0.0f, 0.0f, 0.0f };
    scene_graph::Vec3f _up{ 0.0f, 1.0f, 0.0f };

    static constexpr float _max_phi = 1.5f;// ~86 degrees, avoid gimbal lock
    static constexpr float _min_distance = 0.01f;

    // Mouse tracking state
    double _last_x = 0;
    double _last_y = 0;
    bool _left_down = false;
    bool _right_down = false;
    bool _middle_down = false;

    TouchGestureTracker _touch;

    // Logical window size, 0 until known.
    double _viewport_width = 0;
    double _viewport_height = 0;

    // World units covered by one logical pixel on the plane through the center.
    auto world_per_pixel() const -> float;

    auto compute_eye() const -> scene_graph::Vec3f;

    // Unit screen-right and screen-up directions in world space,
    // including roll.
    auto view_axes() const -> std::pair<scene_graph::Vec3f, scene_graph::Vec3f>;

    // If the active camera is not the default camera, extract orbit
    // parameters from the active camera's world transform, copy them
    // to the default camera, revert to the default camera, and update
    // our spherical state so subsequent orbiting continues from there.
    auto snap_to_active_camera() -> void;

    auto apply() -> void;
};

}// namespace balsa::visualization::vulkan
