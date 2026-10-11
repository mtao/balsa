#if !defined(BALSA_GEOMETRY_SPATIAL_DETAIL_BVHBUILDSTRATEGY_HPP)
#define BALSA_GEOMETRY_SPATIAL_DETAIL_BVHBUILDSTRATEGY_HPP

/// @file detail/BVHBuildStrategy.hpp
/// @brief Enum selecting BVH construction algorithm.

#include <cstdint>

namespace balsa::geometry::spatial {

/// Strategy for BVH construction.
///
/// Each strategy produces a valid binary BVH with single-axis splits
/// and DFS pre-order node layout.  They differ in build speed, tree
/// quality (query performance), and parallelism.
enum class BVHBuildStrategy : uint8_t {
    /// Binned Surface Area Heuristic (default).
    /// O(n log n) build.  Best tree quality — minimizes expected query
    /// cost by choosing splits that minimize the surface-area-weighted
    /// sum of child primitive counts.
    sah,

    /// Object median split.
    /// O(n log n) build.  Splits at the median centroid along the
    /// longest axis, producing balanced trees (depth = ceil(log2 n)).
    /// Moderate tree quality — bounding boxes can overlap heavily.
    object_median,

    /// Spatial median split.
    /// O(n log n) build.  Splits at the spatial midpoint of the
    /// bounding box.  Can produce unbalanced trees if primitives
    /// cluster; falls back to object median for degenerate cases.
    spatial_median,

    /// Linear BVH (Morton-code based).
    /// O(n) build with radix sort.  Embarrassingly parallel.
    /// Lower tree quality (20-40% more node visits vs SAH) but
    /// excellent for very large meshes or real-time rebuild.
    lbvh,
};

} // namespace balsa::geometry::spatial
#endif
