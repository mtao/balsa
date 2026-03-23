#if !defined(BALSA_SCENE_GRAPH_BVH_DATA_HPP)
#define BALSA_SCENE_GRAPH_BVH_DATA_HPP

#include <array>
#include <cstdint>
#include <vector>

#include "AbstractFeature.hpp"
#include "types.hpp"

#include <quiver/spatial/BVH.hpp>
#include <quiver/spatial/KDOP.hpp>
#include <quiver/spatial/KDOPGeometry.hpp>

namespace balsa::scene_graph {

// ── BVHData ─────────────────────────────────────────────────────────
//
// Feature that manages BVH construction and bounding volume overlay
// for a mesh Object.  Attaches to the same Object as MeshData.
//
// When enabled, creates a child Object with wireframe MeshData that
// renders the bounding volume geometry at the selected tree depth.
//
// Configuration:
//   - KDOP type: K=3 (AABB), K=9, K=13
//   - Build strategy: SAH, Object Median, Spatial Median, LBVH
//   - Max leaf size
//   - Display depth (0 = root)
//   - Overlay color
//   - Enabled flag
//
// Deferred update pattern: UI code calls mark_dirty() during the
// render pass; the viewer calls apply_pending_update() before the
// next render pass to rebuild BVH / update wireframe child.

class MeshData;// forward

class BVHData : public AbstractFeature {
  public:
    // ── Configuration ───────────────────────────────────────────────

    // KDOP type: number of half-plane directions.
    // Valid values: 3 (AABB), 9, 13.
    int kdop_k = 3;

    // BVH build strategy.
    quiver::spatial::BVHBuildStrategy strategy =
      quiver::spatial::BVHBuildStrategy::sah;

    // Maximum number of primitives per leaf node.
    uint16_t max_leaf_size = 4;

    // Depth of the tree to display (0 = root level).
    int display_depth = 0;

    // Overlay wireframe color (RGBA).
    float color[4] = { 0.1f, 0.8f, 0.2f, 1.0f };

    // Whether the BVH overlay is enabled (visible).
    bool enabled = true;

    // ── Accessors ───────────────────────────────────────────────────

    // Height of the last-built BVH (-1 if not built).
    int bvh_height() const { return _bvh_height; }

    // Whether BVH data has been built.
    bool is_built() const { return _bvh_height >= 0; }

    // The child Object that holds the wireframe overlay, or nullptr.
    Object *overlay_object() const { return _overlay_obj; }

    // ── Update management ───────────────────────────────────────────

    // Mark the BVH as needing rebuild + overlay update.
    // Safe to call during render pass.
    void mark_dirty() {
        _needs_rebuild = true;
        _needs_update = true;
    }

    // Mark just the overlay as needing update (e.g. depth/color change).
    // Safe to call during render pass.
    void mark_overlay_dirty() { _needs_update = true; }

    // Whether there is a pending update to apply.
    bool needs_update() const { return _needs_update; }

    // Whether this BVHData has been marked for removal.
    // The deferred update loop will remove the overlay, release GPU
    // resources, and remove this feature from the Object.
    bool pending_removal() const { return _pending_removal; }

    // Mark this BVHData for deferred removal.  Safe to call during
    // the render pass — actual cleanup happens in the next
    // begin_render_pass() call (after waitIdle).
    void mark_for_removal() { _pending_removal = true; }

    // Apply pending update: rebuild BVH if needed, then update the
    // wireframe overlay child.  Must be called OUTSIDE the render pass.
    //
    // mesh_data: the MeshData feature on the same Object (caller
    //            provides it to avoid the dynamic_cast each frame).
    //
    // Returns true if anything was updated.
    bool apply_pending_update(MeshData &mesh_data);

    // Remove the overlay child Object.  Must be called outside the
    // render pass.
    void remove_overlay();

  private:
    // ── BVH storage (one per K variant) ─────────────────────────────

    quiver::spatial::BVH<3, 3> _bvh_3;
    quiver::spatial::BVH<3, 9> _bvh_9;
    quiver::spatial::BVH<3, 13> _bvh_13;

    int _bvh_height = -1;

    // ── Overlay state ───────────────────────────────────────────────

    Object *_overlay_obj = nullptr;
    bool _needs_update = false;
    bool _needs_rebuild = false;
    bool _pending_removal = false;

    // ── Internal helpers ────────────────────────────────────────────

    void rebuild_bvh(MeshData &mesh_data);
    void update_overlay();
};

}// namespace balsa::scene_graph

#endif
