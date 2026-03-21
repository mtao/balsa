#if !defined(BALSA_VISUALIZATION_VULKAN_ORBIT_CAMERA_CONTROLLER_HPP)
#define BALSA_VISUALIZATION_VULKAN_ORBIT_CAMERA_CONTROLLER_HPP

#include <algorithm>
#include <cmath>

#include "balsa/scene_graph/types.hpp"
#include "balsa/glm/zipper_compat.hpp"
#include "balsa/visualization/input_handler.hpp"
#include "balsa/visualization/vulkan/mesh_scene.hpp"

namespace balsa::visualization::vulkan {

// ── OrbitCameraController ───────────────────────────────────────────
//
// InputHandler that implements an orbit camera (arcball-style):
//
//   Left-drag   → orbit (rotate around the center point)
//   Right-drag  → pan (translate center + eye in the view plane)
//   Scroll      → zoom (move eye closer/further from center)
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
    explicit OrbitCameraController(MeshScene *scene)
      : _scene(scene) {
        // Initialize _up to (0, 1, 0)
        _up(0) = 0.0f;
        _up(1) = 1.0f;
        _up(2) = 0.0f;
        apply();
    }

    // ── Configuration ───────────────────────────────────────────────

    void set_distance(float d) {
        _distance = std::max(d, _min_distance);
        apply();
    }
    void set_center(const scene_graph::Vec3f &c) {
        _center = c;
        apply();
    }
    void set_angles(float theta, float phi) {
        _theta = theta;
        _phi = std::clamp(phi, -_max_phi, _max_phi);
        apply();
    }

    float distance() const { return _distance; }
    const scene_graph::Vec3f &center() const { return _center; }
    float theta() const { return _theta; }
    float phi() const { return _phi; }

    // Sensitivity parameters
    float orbit_sensitivity = 0.005f;
    float pan_sensitivity = 0.003f;
    float zoom_sensitivity = 0.1f;

    // ── InputHandler overrides ──────────────────────────────────────

    void on_mouse_event(const MouseEvent &e) override {
        switch (e.type) {
        case MouseEvent::Type::Press:
            _last_x = e.x;
            _last_y = e.y;
            if (e.button == 0) _left_down = true;
            if (e.button == 1) _right_down = true;
            if (e.button == 2) _middle_down = true;
            break;

        case MouseEvent::Type::Release:
            if (e.button == 0) _left_down = false;
            if (e.button == 1) _right_down = false;
            if (e.button == 2) _middle_down = false;
            break;

        case MouseEvent::Type::Move: {
            double dx = e.x - _last_x;
            double dy = e.y - _last_y;
            _last_x = e.x;
            _last_y = e.y;

            if (_left_down) {
                // Orbit
                _theta -= static_cast<float>(dx) * orbit_sensitivity;
                _phi += static_cast<float>(dy) * orbit_sensitivity;
                _phi = std::clamp(_phi, -_max_phi, _max_phi);
                apply();
            }

            if (_right_down || _middle_down) {
                // Pan: move center in the view plane
                scene_graph::Vec3f eye = compute_eye();
                scene_graph::Vec3f forward = glm_compat::normalize(_center - eye);
                scene_graph::Vec3f right = glm_compat::normalize(glm_compat::cross(forward, _up));
                scene_graph::Vec3f up = glm_compat::normalize(glm_compat::cross(right, forward));

                float pan_scale = _distance * pan_sensitivity;
                _center = _center - right * (static_cast<float>(dx) * pan_scale);
                _center = _center + up * (static_cast<float>(dy) * pan_scale);
                apply();
            }
            break;
        }

        case MouseEvent::Type::Scroll: {
            // Zoom: exponential scaling feels natural
            float factor = 1.0f - static_cast<float>(e.scroll_y) * zoom_sensitivity;
            _distance = std::max(_distance * factor, _min_distance);
            apply();
            break;
        }
        }
    }

    void on_window_resize(const WindowResizeEvent &e) override {
        if (_scene && e.framebuffer_width > 0 && e.framebuffer_height > 0) {
            _scene->update_aspect(
              static_cast<float>(e.framebuffer_width) / static_cast<float>(e.framebuffer_height));
        }
    }

  private:
    MeshScene *_scene = nullptr;

    // Spherical camera parameters
    float _theta = 0.0f;// azimuthal angle (radians)
    float _phi = 0.3f;// polar angle (radians, 0 = equator)
    float _distance = 3.0f;
    scene_graph::Vec3f _center;// initialized below
    scene_graph::Vec3f _up;// initialized below

    static constexpr float _max_phi = 1.5f;// ~86 degrees, avoid gimbal lock
    static constexpr float _min_distance = 0.01f;

    // Mouse tracking state
    double _last_x = 0;
    double _last_y = 0;
    bool _left_down = false;
    bool _right_down = false;
    bool _middle_down = false;

    scene_graph::Vec3f compute_eye() const {
        float cos_phi = std::cos(_phi);
        float ex = _distance * cos_phi * std::sin(_theta);
        float ey = _distance * std::sin(_phi);
        float ez = _distance * cos_phi * std::cos(_theta);
        scene_graph::Vec3f offset;
        offset(0) = ex;
        offset(1) = ey;
        offset(2) = ez;
        return _center + offset;
    }

    void apply() {
        if (!_scene) return;
        scene_graph::Vec3f eye = compute_eye();
        _scene->look_at(eye, _center, _up);
    }
};

}// namespace balsa::visualization::vulkan

#endif
