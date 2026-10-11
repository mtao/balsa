#if !defined(BALSA_GEOMETRY_SPATIAL_BVH_HPP)
#define BALSA_GEOMETRY_SPATIAL_BVH_HPP

/// @file BVH.hpp
/// @brief Bounding Volume Hierarchy for spatial queries.
///
/// The BVH is a standalone acceleration structure that takes a
/// collection of k-DOP bounding volumes (one per primitive) and
/// organizes them into a binary tree for efficient spatial queries.
///
/// Nodes are stored in DFS pre-order: the left child of node i is
/// always at i + 1 (implicit).  Only the right child index is stored.
/// Leaves reference a contiguous range in a primitive index array.
///
/// The BVH is **not** an attribute — it owns its own storage and is
/// decoupled from the mesh/attribute system.  Build it from a span of
/// k-DOPs (e.g. materialized via CachedAttribute from a
/// BoundingVolumeAttribute).
///
/// @tparam T               Scalar type for bounding volume coordinates.
/// @tparam SpatialDim      Embedding dimension (2 or 3).
/// @tparam K               Number of k-DOP slab directions.
/// @tparam DirectionPolicy Direction vector policy.

#include <cstdint>
#include <functional>
#include <limits>
#include <optional>
#include <span>
#include <utility>
#include <vector>

#include <balsa/geometry/spatial/KDOP.hpp>
#include <balsa/geometry/spatial/detail/BVHBuildStrategy.hpp>
#include <balsa/geometry/spatial/detail/BVHNode.hpp>

namespace balsa::geometry::spatial {

/// Configuration for BVH construction.
struct BVHConfig {
    /// Construction algorithm.
    BVHBuildStrategy strategy = BVHBuildStrategy::sah;

    /// Maximum number of primitives per leaf node.
    /// Lower values produce deeper trees (better query pruning) at the
    /// cost of more nodes and higher build time.
    uint16_t max_leaf_size = 4;
};

/// Bounding Volume Hierarchy for spatial queries.
///
/// Supports 4 construction strategies, 3 query types, and dynamic
/// maintenance (refit, incremental insert/remove, full rebuild).
template <typename T,
          int8_t SpatialDim,
          int8_t K,
          typename DirectionPolicy = DefaultDirections<SpatialDim, K>>
class BVH {
    // ring<T> suffices for the BVH container: it stores KDOPs and performs
    // overlap/containment/distance queries that only use +, -, *.
    // The build() method dispatches to builder strategies that each
    // independently require field<T> (via their own static_assert).
    static_assert(ring<T>,
                  "BVH requires ring<T> (arithmetic type with +, -, *).");

  public:
    using kdop_type = KDOP<T, SpatialDim, K, DirectionPolicy>;
    using node_type = detail::BVHNode<T, SpatialDim, K, DirectionPolicy>;

    // -- Construction
    // ----------------------------------------------------------

    /// Default-construct an empty BVH.
    BVH() = default;

    /// Build a BVH from a collection of bounding volumes.
    ///
    /// @param bounds  One k-DOP per primitive (indexed 0..N-1).
    /// @param config  Build configuration (strategy, max leaf size).
    void build(std::span<const kdop_type> bounds, const BVHConfig &config = {});

    /// Rebuild the tree from scratch with new bounds, preserving config.
    ///
    /// Equivalent to calling build() with the same config used for the
    /// most recent build.
    void rebuild(std::span<const kdop_type> bounds);

    // -- Queries
    // ---------------------------------------------------------------

    /// Find all primitives whose bounding volumes overlap @p query_bounds.
    ///
    /// @param query_bounds  The query bounding volume.
    /// @param callback      Called with each overlapping primitive index.
    void query_overlap(const kdop_type &query_bounds,
                       const std::function<void(uint32_t)> &callback) const;

    /// Find all primitives whose bounding volumes contain @p point.
    ///
    /// @param point     The query point.
    /// @param callback  Called with each primitive whose BV contains the point.
    void query_point(std::span<const T, SpatialDim> point,
                     const std::function<void(uint32_t)> &callback) const;

    /// Find the nearest primitive to @p point using bounding volume
    /// distance pruning.
    ///
    /// @param point          The query point.
    /// @param distance_func  Returns the exact squared distance from the
    ///                       query point to primitive @p index.  Called
    ///                       only for primitives that survive BV pruning.
    /// @return  The index of the nearest primitive and its squared distance,
    ///          or {uint32_t(-1), infinity} if the BVH is empty.
    auto query_nearest(std::span<const T, SpatialDim> point,
                       const std::function<T(uint32_t)> &distance_func) const
        -> std::pair<uint32_t, T>;

    /// Find all primitives whose bounding volumes are intersected by a ray.
    ///
    /// Traverses the BVH using ray-KDOP slab tests, visiting all
    /// primitives whose bounding volumes are pierced by the ray.
    /// Uses the stored `split_axis` for front-to-back child ordering
    /// when the ray direction has a positive component along that axis.
    ///
    /// @param origin     Ray origin.
    /// @param direction  Ray direction (not necessarily normalized).
    /// @param hit_func   Called for each candidate primitive.  Should
    ///                   return the ray t-parameter if the ray actually
    ///                   hits the primitive, or std::nullopt if it misses.
    /// @param callback   Called with (primitive_index, t) for each hit.
    void query_ray(std::span<const T, SpatialDim> origin,
                   std::span<const T, SpatialDim> direction,
                   const std::function<std::optional<T>(uint32_t)> &hit_func,
                   const std::function<void(uint32_t, T)> &callback) const;

    /// Find the nearest ray hit among all primitives.
    ///
    /// Like query_ray, but maintains a running t_max for early
    /// termination.  Only reports the single closest hit.
    ///
    /// @param origin     Ray origin.
    /// @param direction  Ray direction (not necessarily normalized).
    /// @param hit_func   Returns the ray t-parameter if the ray hits the
    ///                   primitive (must return t >= 0), or std::nullopt.
    /// @return  (primitive_index, t) of the nearest hit, or
    ///          (uint32_t(-1), infinity) if no hit.
    auto query_ray_nearest(
        std::span<const T, SpatialDim> origin,
        std::span<const T, SpatialDim> direction,
        const std::function<std::optional<T>(uint32_t)> &hit_func) const
        -> std::pair<uint32_t, T>;

    /// Find all primitives within a squared radius of a point.
    ///
    /// Uses bounding volume min_distance_sq for pruning.
    ///
    /// @param point          The query point.
    /// @param radius_sq      Squared search radius.
    /// @param distance_func  Returns the exact squared distance from the
    ///                       query point to primitive @p index.
    /// @param callback       Called with (primitive_index, distance_sq)
    ///                       for each primitive within the radius.
    void query_within_radius(
        std::span<const T, SpatialDim> point,
        T radius_sq,
        const std::function<T(uint32_t)> &distance_func,
        const std::function<void(uint32_t, T)> &callback) const;

    /// Find the k nearest primitives to a point.
    ///
    /// Maintains a max-heap of size k, using the heap top as the
    /// pruning threshold.
    ///
    /// @param point          The query point.
    /// @param k              Number of nearest primitives to find.
    /// @param distance_func  Returns the exact squared distance from the
    ///                       query point to primitive @p index.
    /// @return  Vector of (primitive_index, distance_sq) pairs, sorted
    ///          by ascending distance.  May contain fewer than k entries
    ///          if the BVH has fewer than k primitives.
    auto query_k_nearest(std::span<const T, SpatialDim> point,
                         uint32_t k,
                         const std::function<T(uint32_t)> &distance_func) const
        -> std::vector<std::pair<uint32_t, T>>;

    /// Find all overlapping pairs between this BVH and another BVH.
    ///
    /// Performs dual-tree descent, testing pairs of nodes from each tree.
    /// At each step, the larger node (by primitive count) is split.
    /// Both BVHs must use the same KDOP type.
    ///
    /// @param other     The other BVH to test against.
    /// @param callback  Called with (index_a, index_b) for each pair of
    ///                  primitives whose bounding volumes overlap.
    void query_overlap_bvh(
        const BVH &other,
        const std::function<void(uint32_t, uint32_t)> &callback) const;

    // -- Dynamic maintenance
    // ---------------------------------------------------

    /// Refit the tree by recomputing internal node bounds bottom-up.
    ///
    /// Use after primitive bounds have been updated in place.
    /// The tree structure (topology) is unchanged — only bounds are
    /// updated.  O(n) time.
    ///
    /// @param bounds  Updated bounding volumes (same count as the
    ///                original build).
    void refit(std::span<const kdop_type> bounds);

    /// Insert a new primitive into the tree.
    ///
    /// Finds the best leaf to accommodate the new primitive (by surface
    /// area cost) and splits it if needed.  Does NOT guarantee optimal
    /// tree quality — call rebuild() periodically for best performance.
    ///
    /// @param index   Index of the new primitive.
    /// @param bounds  Bounding volume of the new primitive.
    void insert(uint32_t index, const kdop_type &bounds);

    /// Remove a primitive from the tree.
    ///
    /// Marks the primitive as removed and collapses degenerate nodes.
    /// Does NOT compact the primitive index array — the tree will
    /// accumulate dead references until rebuild().
    ///
    /// @param index  Index of the primitive to remove.
    void remove(uint32_t index);

    // -- Accessors
    // -------------------------------------------------------------

    /// Returns true if the BVH has been built.
    [[nodiscard]] auto is_built() const -> bool;

    /// Total number of nodes in the tree.
    [[nodiscard]] auto node_count() const -> size_t;

    /// Number of primitives in the tree.
    [[nodiscard]] auto primitive_count() const -> size_t;

    /// Read-only access to the node array (DFS pre-order).
    [[nodiscard]] auto nodes() const -> std::span<const node_type>;

    /// Read-only access to the primitive index array.
    [[nodiscard]] auto primitive_indices() const -> std::span<const uint32_t>;

    /// Root bounding volume (enclosing all primitives).
    [[nodiscard]] auto root_bounds() const -> const kdop_type &;

    /// Height of the tree (0 for a single leaf, -1 if empty).
    [[nodiscard]] auto height() const -> int32_t;

    /// Configuration used for the last build.
    [[nodiscard]] auto config() const -> const BVHConfig &;

  private:
    std::vector<node_type> m_nodes;
    std::vector<uint32_t> m_prim_indices;
    std::vector<kdop_type> m_leaf_bounds;
    BVHConfig m_config;

    /// Compute height recursively from node @p idx.
    [[nodiscard]] auto height_recursive(uint32_t idx) const -> int32_t;
};

// ============================================================================
// Convenience aliases
// ============================================================================

/// BVH using axis-aligned bounding boxes.
template <typename T, int8_t SpatialDim>
using AABB_BVH = BVH<T, SpatialDim, SpatialDim>;

} // namespace balsa::geometry::spatial

#include "BVH.hxx"
#endif
