#include "balsa/scene_graph/Camera.hpp"
#include "balsa/scene_graph/Object.hpp"

#include <cmath>

#include <zipper/transform/projection.hpp>

namespace balsa::scene_graph {

// ── Projection setup ────────────────────────────────────────────────

void Camera::set_perspective(float fov_y, float aspect, float near, float far) {
    _projection_type = Projection::Perspective;
    _fov_y = fov_y;
    _aspect = aspect;
    _near = near;
    _far = far;
    recompute_perspective();
}

void Camera::set_orthographic(float left, float right, float bottom, float top, float near, float far) {
    _projection_type = Projection::Orthographic;
    _near = near;
    _far = far;
    auto p = ::zipper::transform::ortho(left, right, bottom, top, near, far);
    // Vulkan clip space: Y inverted vs OpenGL.
    p(1, 1) *= -1.0f;
    _projection = p;
}

void Camera::set_fov_axis(FovAxis axis) {
    _fov_axis = axis;
    if (_projection_type == Projection::Perspective) {
        recompute_perspective();
    }
}

float Camera::vertical_fov() const {
    if (_fov_axis == FovAxis::ShorterSide && _aspect > 0.0f && _aspect < 1.0f) {
        // Portrait: the fov spans the width; widen the vertical angle.
        return 2.0f * std::atan(std::tan(0.5f * _fov_y) / _aspect);
    }
    return _fov_y;
}

void Camera::update_aspect(float aspect) {
    _aspect = aspect;
    if (_projection_type == Projection::Perspective) {
        recompute_perspective();
    }
}

// ── Derived matrices ────────────────────────────────────────────────

Mat4f Camera::view_matrix() const {
    return object().world_transform().inverse().to_matrix();
}

// ── Private ─────────────────────────────────────────────────────────

void Camera::recompute_perspective() {
    auto p = ::zipper::transform::perspective(vertical_fov(), _aspect, _near, _far);
    // Vulkan clip space: Y inverted vs OpenGL.
    p(1, 1) *= -1.0f;
    _projection = p;
}

}// namespace balsa::scene_graph
