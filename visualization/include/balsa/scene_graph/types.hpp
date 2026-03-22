#if !defined(BALSA_SCENE_GRAPH_TYPES_HPP)
#define BALSA_SCENE_GRAPH_TYPES_HPP

// Scene graph type aliases.
//
// The scene graph needs column-major matrices to match Vulkan/GLSL and
// GPU memory layout conventions.  Rather than changing the global
// balsa::Matrix default, we define explicit column-major aliases here.
//
// These types resolve through:
//   scene_graph::Mat4f
//     -> balsa::zipper::Matrix<float, 4, 4, /*IsRowMajor=*/false>
//        -> ::zipper::Matrix<float, 4, 4, false>

#include <balsa/zipper/types.hpp>
#include <zipper/Quaternion.hpp>
#include <zipper/transform/Transform.hpp>
#include <zipper/transform/Translation.hpp>
#include <zipper/transform/Rotation.hpp>
#include <zipper/transform/Scaling.hpp>
#include <zipper/transform/model.hpp>
#include <zipper/transform/quaternion_transform.hpp>

namespace balsa::scene_graph {

// ── Column-major matrices ──────────────────────────────────────────
using Mat4f = balsa::zipper::Matrix<float, 4, 4, /*IsRowMajor=*/false>;
using Mat3f = balsa::zipper::Matrix<float, 3, 3, /*IsRowMajor=*/false>;

// ── Vectors ────────────────────────────────────────────────────────
using Vec2f = balsa::zipper::Vector<float, 2>;
using Vec3f = balsa::zipper::Vector<float, 3>;
using Vec4f = balsa::zipper::Vector<float, 4>;

// ── Full-matrix transform aliases ──────────────────────────────────
using AffineTransformf = ::zipper::transform::AffineTransform<float>;
using Isometry3f = ::zipper::transform::Isometry<float, 3>;
using ProjectiveTransform3f = ::zipper::transform::ProjectiveTransform<float, 3>;

// ── Specialized (minimal-storage) transform aliases ────────────────
using Translation3f = ::zipper::transform::Translation<float, 3>;
using Rotation3f = ::zipper::transform::Rotation<float, 3>;
using Scaling3f = ::zipper::transform::Scaling<float, 3>;

// ── Quaternion ─────────────────────────────────────────────────────
using Quaternionf = ::zipper::Quaternion<float>;

}// namespace balsa::scene_graph

#endif
