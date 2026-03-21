#if !defined(BALSA_SCENE_GRAPH_TYPES_HPP)
#define BALSA_SCENE_GRAPH_TYPES_HPP

// Scene graph type aliases.
//
// The scene graph needs column-major matrices to match GLM, Vulkan/GLSL,
// and GPU memory layout conventions.  Rather than changing the global
// balsa::Matrix default, we define explicit column-major aliases here.
//
// These types resolve through:
//   scene_graph::Mat4f
//     -> balsa::zipper::Matrix<float, 4, 4, /*IsRowMajor=*/false>
//        -> ::zipper::Matrix<float, 4, 4, false>

#include <balsa/zipper/types.hpp>

namespace balsa::scene_graph {

// ── Column-major matrices ──────────────────────────────────────────
// Explicitly IsRowMajor=false so that in-memory layout matches
// glm::mat4 (column-major) and can be memcpy'd to/from GPU buffers.

using Mat4f = balsa::zipper::Matrix<float, 4, 4, /*IsRowMajor=*/false>;
using Mat3f = balsa::zipper::Matrix<float, 3, 3, /*IsRowMajor=*/false>;

// ── Vectors ────────────────────────────────────────────────────────
// Vectors don't have a row/column-major distinction (they're 1D),
// but we alias them here for convenience so that scene_graph code
// doesn't need to pull in the full tensor_types header.

using Vec2f = balsa::zipper::Vector<float, 2>;
using Vec3f = balsa::zipper::Vector<float, 3>;
using Vec4f = balsa::zipper::Vector<float, 4>;

}// namespace balsa::scene_graph

#endif
