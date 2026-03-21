#include "balsa/glm/zipper_compat.hpp"

#include <cstring>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace balsa::glm_compat {

// ── Mat4 conversions ────────────────────────────────────────────────
// Both column-major, so memcpy is correct.

::glm::mat4 as_glm(const Mat4f &m) {
    ::glm::mat4 result;
    std::memcpy(::glm::value_ptr(result), m.expression().data(), sizeof(float) * 16);
    return result;
}

Mat4f as_zipper(const ::glm::mat4 &m) {
    Mat4f result;
    std::memcpy(result.expression().data(), ::glm::value_ptr(m), sizeof(float) * 16);
    return result;
}

// ── Mat3 conversions ────────────────────────────────────────────────

::glm::mat3 as_glm(const Mat3f &m) {
    ::glm::mat3 result;
    std::memcpy(::glm::value_ptr(result), m.expression().data(), sizeof(float) * 9);
    return result;
}

Mat3f as_zipper(const ::glm::mat3 &m) {
    Mat3f result;
    std::memcpy(result.expression().data(), ::glm::value_ptr(m), sizeof(float) * 9);
    return result;
}

// ── Vec4 conversions ────────────────────────────────────────────────

::glm::vec4 as_glm(const Vec4f &v) {
    return ::glm::vec4(v(0), v(1), v(2), v(3));
}

Vec4f as_zipper(const ::glm::vec4 &v) {
    Vec4f result;
    result(0) = v.x;
    result(1) = v.y;
    result(2) = v.z;
    result(3) = v.w;
    return result;
}

// ── Vec3 conversions ────────────────────────────────────────────────

::glm::vec3 as_glm(const Vec3f &v) {
    return ::glm::vec3(v(0), v(1), v(2));
}

Vec3f as_zipper(const ::glm::vec3 &v) {
    Vec3f result;
    result(0) = v.x;
    result(1) = v.y;
    result(2) = v.z;
    return result;
}

// ── Vec2 conversions ────────────────────────────────────────────────

::glm::vec2 as_glm(const Vec2f &v) {
    return ::glm::vec2(v(0), v(1));
}

Vec2f as_zipper(const ::glm::vec2 &v) {
    Vec2f result;
    result(0) = v.x;
    result(1) = v.y;
    return result;
}


// ── Projection matrices ─────────────────────────────────────────────
// TODO(zipper): These should be native zipper operations.

Mat4f perspective(float fov_y, float aspect, float near_plane, float far_plane) {
    auto p = ::glm::perspective(fov_y, aspect, near_plane, far_plane);
    // Vulkan clip space: Y inverted vs OpenGL.
    p[1][1] *= -1.0f;
    return as_zipper(p);
}

Mat4f ortho(float left, float right, float bottom, float top, float near_plane, float far_plane) {
    auto p = ::glm::ortho(left, right, bottom, top, near_plane, far_plane);
    // Vulkan clip space: Y inverted vs OpenGL.
    p[1][1] *= -1.0f;
    return as_zipper(p);
}

// ── View matrix ─────────────────────────────────────────────────────

Mat4f look_at(const Vec3f &eye, const Vec3f &center, const Vec3f &up) {
    return as_zipper(::glm::lookAt(as_glm(eye), as_glm(center), as_glm(up)));
}

// ── Matrix operations ───────────────────────────────────────────────

Mat4f inverse(const Mat4f &m) {
    return as_zipper(::glm::inverse(as_glm(m)));
}

Mat4f inverse_transpose(const Mat4f &m) {
    return as_zipper(::glm::inverseTranspose(as_glm(m)));
}

Mat4f translate(const Mat4f &m, const Vec3f &v) {
    return as_zipper(::glm::translate(as_glm(m), as_glm(v)));
}

Mat4f identity4f() {
    return as_zipper(::glm::mat4(1.0f));
}

// ── Vector operations ───────────────────────────────────────────────
// TODO(zipper): normalize, cross, dot should be native zipper operations.

Vec3f normalize(const Vec3f &v) {
    return as_zipper(::glm::normalize(as_glm(v)));
}

Vec3f cross(const Vec3f &a, const Vec3f &b) {
    return as_zipper(::glm::cross(as_glm(a), as_glm(b)));
}

float dot(const Vec3f &a, const Vec3f &b) {
    return ::glm::dot(as_glm(a), as_glm(b));
}

}// namespace balsa::glm_compat
