#include "balsa/scene_graph/Camera.hpp"
#include "balsa/scene_graph/Object.hpp"

#include "balsa/glm/zipper_compat.hpp"

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
    _projection = glm_compat::ortho(left, right, bottom, top, near, far);
}

void Camera::update_aspect(float aspect) {
    _aspect = aspect;
    if (_projection_type == Projection::Perspective) {
        recompute_perspective();
    }
}

// ── Derived matrices ────────────────────────────────────────────────

Mat4f Camera::view_matrix() const {
    return glm_compat::inverse(object().world_transform());
}

// ── Private ─────────────────────────────────────────────────────────

void Camera::recompute_perspective() {
    _projection = glm_compat::perspective(_fov_y, _aspect, _near, _far);
}

}// namespace balsa::scene_graph
