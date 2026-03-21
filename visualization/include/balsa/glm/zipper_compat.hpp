#if !defined(BALSA_GLM_ZIPPER_COMPAT_HPP)
#define BALSA_GLM_ZIPPER_COMPAT_HPP

// GLM <-> Zipper compatibility layer.
//
// Provides bidirectional type conversions and wraps GLM operations that
// should eventually become native zipper operations.  Functions marked
// "TODO(zipper)" are candidates for upstreaming into the zipper library.
//
// Conversions are memcpy-based: both glm and the scene_graph zipper
// matrices use column-major storage, so the in-memory layouts are
// identical.
//
// Types used here are from balsa::scene_graph (which explicitly
// specifies column-major layout) rather than the global balsa::
// aliases, whose layout may differ.

#include <balsa/scene_graph/types.hpp>

#include <glm/mat3x3.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

namespace balsa::glm_compat {

// Import the scene_graph types we operate on.
using scene_graph::Mat3f;
using scene_graph::Mat4f;
using scene_graph::Vec2f;
using scene_graph::Vec3f;
using scene_graph::Vec4f;

// ── Mat4 conversions ───────────────────────────────────────────────

::glm::mat4 as_glm(const Mat4f &m);
Mat4f as_zipper(const ::glm::mat4 &m);

// ── Mat3 conversions ───────────────────────────────────────────────

::glm::mat3 as_glm(const Mat3f &m);
Mat3f as_zipper(const ::glm::mat3 &m);

// ── Vec4 conversions ───────────────────────────────────────────────

::glm::vec4 as_glm(const Vec4f &v);
Vec4f as_zipper(const ::glm::vec4 &v);

// ── Vec3 conversions ───────────────────────────────────────────────

::glm::vec3 as_glm(const Vec3f &v);
Vec3f as_zipper(const ::glm::vec3 &v);

// ── Vec2 conversions ───────────────────────────────────────────────

::glm::vec2 as_glm(const Vec2f &v);
Vec2f as_zipper(const ::glm::vec2 &v);


// ── Wrapped operations ─────────────────────────────────────────────
// TODO(zipper): These should eventually be native zipper operations.
// Internally they convert to glm, call the glm function, and convert
// back.  The round-trip cost is negligible for transform-sized (4x4)
// matrices.

// Projection matrices (Vulkan clip space: Y inverted).
Mat4f perspective(float fov_y, float aspect, float near_plane, float far_plane);
Mat4f ortho(float left, float right, float bottom, float top, float near_plane, float far_plane);

// View matrix.
Mat4f look_at(const Vec3f &eye, const Vec3f &center, const Vec3f &up);

// Matrix inverse.
Mat4f inverse(const Mat4f &m);

// Inverse-transpose (for transforming normals).
Mat4f inverse_transpose(const Mat4f &m);

// Translation.
Mat4f translate(const Mat4f &m, const Vec3f &v);

// 4x4 identity matrix (convenience -- can also default-construct).
Mat4f identity4f();

// Vector operations.
// TODO(zipper): normalize, cross, dot should be native.
Vec3f normalize(const Vec3f &v);
Vec3f cross(const Vec3f &a, const Vec3f &b);
float dot(const Vec3f &a, const Vec3f &b);

}// namespace balsa::glm_compat

#endif
