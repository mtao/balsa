// bvh_visualization.hpp — DEPRECATED
//
// The monolithic BVHOverlay class has been replaced by the per-mesh
// scene_graph::BVHData feature.  See:
//   - balsa/scene_graph/BVHData.hpp       (feature class)
//   - balsa/scene_graph/BVHData.cpp       (implementation)
//
// BVH overlays are now added/removed per-mesh via context menu in
// both ImGui and Qt viewers.  MeshScene::begin_render_pass() handles
// deferred BVH updates automatically.
//
// This header is kept as a tombstone to avoid silent breakage for any
// out-of-tree code that may include it.  It provides nothing.

#if !defined(BALSA_VISUALIZATION_VULKAN_BVH_VISUALIZATION_HPP)
#define BALSA_VISUALIZATION_VULKAN_BVH_VISUALIZATION_HPP

#pragma message("bvh_visualization.hpp is deprecated — use balsa/scene_graph/BVHData.hpp instead")

#endif
