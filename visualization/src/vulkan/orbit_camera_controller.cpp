#include "balsa/visualization/vulkan/orbit_camera_controller.hpp"

#include <algorithm>
#include <cmath>

#include "balsa/visualization/vulkan/mesh_scene.hpp"

namespace balsa::visualization::vulkan {

OrbitCameraController::OrbitCameraController(MeshScene *scene)
  : _scene(scene) {
    apply();
}

auto OrbitCameraController::set_distance(float d) -> void {
    _distance = std::max(d, _min_distance);
    apply();
}

auto OrbitCameraController::set_center(const scene_graph::Vec3f &c) -> void {
    _center = c;
    apply();
}

auto OrbitCameraController::set_angles(float theta, float phi) -> void {
    _theta = theta;
    _phi = std::clamp(phi, -_max_phi, _max_phi);
    apply();
}

auto OrbitCameraController::set_roll(float roll) -> void {
    _roll = roll;
    apply();
}

auto OrbitCameraController::frame(const geometry::BoundingBox<float, 3> &box) -> void {
    if (!_scene || box.is_empty()) return;
    _scene->set_active_camera(nullptr);
    const auto &camera = _scene->camera();
    const float radius = std::max(0.5f * box.range().norm(), 1e-6f);
    // Fit against the narrower of the two view angles.
    const float tan_v = std::tan(0.5f * camera.vertical_fov());
    const float half_fov = std::atan(std::min(tan_v, tan_v * camera.aspect()));
    _center = box.center();
    _roll = 0.0f;
    _distance = std::max(1.05f * radius / std::sin(half_fov), _min_distance);
    _scene->set_perspective(camera.fov_y(),
                            camera.aspect(),
                            1e-3f * radius,
                            100.f * (_distance + radius));
    apply();
}

auto OrbitCameraController::on_mouse_event(const MouseEvent &e) -> void {
    switch (e.type) {
    case MouseEvent::Type::Press:
        _last_x = e.x;
        _last_y = e.y;
        if (e.button == 0) _left_down = true;
        if (e.button == 1) _right_down = true;
        if (e.button == 2) _middle_down = true;
        // When a non-default camera is active, snap the default
        // camera to match it and revert to the default on first
        // interaction.  The non-default camera acts as a bookmark.
        snap_to_active_camera();
        break;

    case MouseEvent::Type::Release:
        if (e.button == 0) _left_down = false;
        if (e.button == 1) _right_down = false;
        if (e.button == 2) _middle_down = false;
        break;

    case MouseEvent::Type::Move: {
        // Trust the held-button state when the backend reports it, so a
        // release delivered to another window cannot leave a drag stuck.
        if (e.buttons >= 0) {
            _left_down = e.buttons & (1 << 0);
            _right_down = e.buttons & (1 << 1);
            _middle_down = e.buttons & (1 << 2);
        }
        double dx = e.x - _last_x;
        double dy = e.y - _last_y;
        _last_x = e.x;
        _last_y = e.y;

        if (_left_down) {
            orbit(dx, dy);
        }

        if (_right_down || _middle_down) {
            pan(dx, dy);
        }
        break;
    }

    case MouseEvent::Type::Scroll: {
        // Snap from bookmark camera on first interaction.
        snap_to_active_camera();
        // Zoom: exponential scaling feels natural
        zoom_at(1.0f / (1.0f - static_cast<float>(e.scroll_y) * zoom_sensitivity), e.x, e.y);
        break;
    }
    }
}

auto OrbitCameraController::on_touch_event(const TouchEvent &e) -> void {
    if (e.type == TouchEvent::Type::Begin) {
        snap_to_active_camera();
    }
    const auto gesture = _touch.update(e);
    if (!gesture) return;
    const double dx = gesture->translation(0);
    const double dy = gesture->translation(1);
    if (gesture->touch_count == 1) {
        orbit(dx * touch_orbit_scale, dy * touch_orbit_scale);
    } else {
        pan(dx, dy);
        zoom_at(static_cast<float>(gesture->scale),
                gesture->centroid(0),
                gesture->centroid(1));
        roll_at(gesture->rotation, gesture->centroid(0), gesture->centroid(1));
    }
}

auto OrbitCameraController::on_touchpad_event(const TouchpadEvent &e) -> void {
    snap_to_active_camera();
    switch (e.type) {
    case TouchpadEvent::Type::Scroll:
        if (e.modifiers & ModControl) {
            zoom_at(static_cast<float>(std::exp(e.dy * touchpad_zoom_sensitivity)), e.x, e.y);
        } else if (e.modifiers & ModShift) {
            pan(e.dx, e.dy);
        } else {
            orbit(e.dx * touchpad_orbit_scale, e.dy * touchpad_orbit_scale);
        }
        break;
    case TouchpadEvent::Type::Pinch:
        zoom_at(static_cast<float>(e.scale), e.x, e.y);
        break;
    case TouchpadEvent::Type::Swipe:
        pan(e.dx, e.dy);
        break;
    case TouchpadEvent::Type::Rotate:
        roll_at(e.angle * touchpad_roll_scale, e.x, e.y);
        break;
    }
}

auto OrbitCameraController::on_window_resize(const WindowResizeEvent &e) -> void {
    set_viewport_size(e.width, e.height);
    if (_scene && e.framebuffer_width > 0 && e.framebuffer_height > 0) {
        _scene->update_aspect(
          static_cast<float>(e.framebuffer_width) / static_cast<float>(e.framebuffer_height));
    }
}

auto OrbitCameraController::on_focus_lost() -> void {
    _left_down = _right_down = _middle_down = false;
    _touch.reset();
}

auto OrbitCameraController::orbit(double dx, double dy) -> void {
    // Express the drag in the un-rolled screen frame so it still follows
    // the cursor when the view is rolled.
    const double c = std::cos(_roll);
    const double s = std::sin(_roll);
    const double ux = dx * c + dy * s;
    const double uy = dy * c - dx * s;
    _theta -= static_cast<float>(ux) * orbit_sensitivity;
    _phi = std::clamp(_phi + static_cast<float>(uy) * orbit_sensitivity, -_max_phi, _max_phi);
    apply();
}

auto OrbitCameraController::pan(double dx, double dy) -> void {
    const auto [right, up] = view_axes();
    const float pan_scale = world_per_pixel();
    _center = _center - right * (static_cast<float>(dx) * pan_scale);
    _center = _center + up * (static_cast<float>(dy) * pan_scale);
    apply();
}

auto OrbitCameraController::zoom(float factor) -> void {
    if (!(factor > 0.0f)) return;
    _distance = std::max(_distance / factor, _min_distance);
    apply();
}

auto OrbitCameraController::zoom_at(float factor, double x, double y) -> void {
    if (_viewport_width <= 0 || _viewport_height <= 0) {
        zoom(factor);
        return;
    }
    const float before = world_per_pixel();
    zoom(factor);
    // The point under (x, y) moved away from the viewport center by a
    // factor of before/after; pan it back under the cursor.
    const double shift = 1.0 - static_cast<double>(before / world_per_pixel());
    pan((x - 0.5 * _viewport_width) * shift, (y - 0.5 * _viewport_height) * shift);
}

auto OrbitCameraController::roll_at(double angle, double x, double y) -> void {
    if (angle == 0.0) return;
    _roll += static_cast<float>(angle);
    apply();
    if (_viewport_width <= 0 || _viewport_height <= 0) return;
    // The point under (x, y) turned with the view about the viewport
    // center; pan it back under the cursor.
    const double ox = x - 0.5 * _viewport_width;
    const double oy = y - 0.5 * _viewport_height;
    const double c = std::cos(angle);
    const double s = std::sin(angle);
    const double rx = ox * c - oy * s;
    const double ry = ox * s + oy * c;
    pan(ox - rx, oy - ry);
}

auto OrbitCameraController::set_viewport_size(double width, double height) -> void {
    _viewport_width = width;
    _viewport_height = height;
}

auto OrbitCameraController::world_per_pixel() const -> float {
    if (!_scene || _viewport_height <= 0) return _distance * pan_sensitivity;
    const auto &camera = _scene->camera();
    // |projection(1,1)| is 1/tan(fov/2) for perspective and 2/(top-bottom)
    // for orthographic (negated for Vulkan's flipped y); the visible height at the center is 2/P11, times
    // the distance for perspective.
    const float p11 = std::abs(camera.projection_matrix()(1, 1));
    if (!(p11 > 0.0f)) return _distance * pan_sensitivity;
    float visible_height = 2.0f / p11;
    if (camera.projection_type() == scene_graph::Camera::Projection::Perspective) {
        visible_height *= _distance;
    }
    return visible_height / static_cast<float>(_viewport_height);
}

auto OrbitCameraController::compute_eye() const -> scene_graph::Vec3f {
    const float cos_phi = std::cos(_phi);
    return _center + scene_graph::Vec3f{ _distance * cos_phi * std::sin(_theta),
                                         _distance * std::sin(_phi),
                                         _distance * cos_phi * std::cos(_theta) };
}

auto OrbitCameraController::view_axes() const
  -> std::pair<scene_graph::Vec3f, scene_graph::Vec3f> {
    const scene_graph::Vec3f forward = (_center - compute_eye()).normalized();
    const scene_graph::Vec3f right0 = forward.cross(_up).normalized();
    const scene_graph::Vec3f up0 = right0.cross(forward).normalized();
    const float c = std::cos(_roll);
    const float s = std::sin(_roll);
    return { right0 * c + up0 * s, up0 * c - right0 * s };
}

auto OrbitCameraController::snap_to_active_camera() -> void {
    if (!_scene || _scene->is_default_camera_active()) return;

    // Extract eye position from the active camera's world transform.
    auto world_xf = _scene->active_camera_object().world_transform();
    scene_graph::Vec3f eye(world_xf.translation());

    // Extract forward direction (camera looks along -Z in its local space).
    scene_graph::Vec3f forward = (-world_xf.linear().col(2)).normalized();

    // Derive center from eye + forward * distance.
    _center = eye + forward * _distance;

    // Compute spherical coordinates from (eye - center).
    scene_graph::Vec3f offset = eye - _center;
    _distance = offset.norm();
    _distance = std::max(_distance, _min_distance);

    _phi = std::asin(std::clamp(offset(1) / _distance, -1.0f, 1.0f));
    _theta = std::atan2(offset(0), offset(2));

    // Roll: the camera's up relative to the un-rolled up.
    _roll = 0.0f;
    const auto [right0, up0] = view_axes();
    const scene_graph::Vec3f camera_up = world_xf.linear().col(1);
    _roll = std::atan2(-camera_up.dot(right0), camera_up.dot(up0));

    // Snap the default camera to this position and revert.
    _scene->set_active_camera(nullptr);
    apply();
}

auto OrbitCameraController::apply() -> void {
    if (!_scene) return;
    _scene->look_at(compute_eye(), _center, view_axes().second);
}

}// namespace balsa::visualization::vulkan
