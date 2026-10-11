#if !defined(BALSA_GEOMETRY_SPATIAL_DETAIL_OBJECTMEDIANBUILDER_HPP)
#define BALSA_GEOMETRY_SPATIAL_DETAIL_OBJECTMEDIANBUILDER_HPP

/// @file detail/ObjectMedianBuilder.hpp
/// @brief Object median BVH construction.
///
/// Splits at the median centroid along the longest axis of the
/// centroid bounding box.  Produces balanced trees with depth
/// ceil(log2 n).  Moderate tree quality — bounding boxes can overlap
/// heavily — but fast and simple.

#include <algorithm>
#include <cstdint>
#include <numeric>
#include <span>
#include <vector>

#include <balsa/geometry/spatial/detail/BVHNode.hpp>
#include <balsa/parallel.hpp>

namespace balsa::geometry::spatial::detail {

/// Build context for object median construction.  Operates on an
/// index array that gets reordered during construction.
template <typename T, int8_t SpatialDim, int8_t K, typename DirectionPolicy>
struct ObjectMedianBuilder {
    static_assert(field<T>,
                  "ObjectMedianBuilder requires field<T> "
                  "(centroid computation involves division)");

    using node_type = BVHNode<T, SpatialDim, K, DirectionPolicy>;
    using kdop_type = typename node_type::kdop_type;

    std::span<const kdop_type> leaf_bounds;
    std::vector<uint32_t> &prim_indices;
    std::vector<node_type> &nodes;
    uint16_t max_leaf_size;

    /// Precomputed centroids (axis-aligned only, first SpatialDim slabs).
    std::vector<std::array<T, SpatialDim>> centroids;

    void precompute_centroids() {
        centroids.resize(leaf_bounds.size());
        parallel::for_each(size_t{0}, leaf_bounds.size(), [&](size_t i) {
            centroids[i] = leaf_bounds[i].center();
        });
    }

    /// Build the subtree for primitives in [begin, end) of prim_indices.
    /// Returns the index of the created node.
    auto build_recursive(uint32_t begin, uint32_t end) -> uint32_t {
        uint32_t count = end - begin;
        uint32_t node_idx = static_cast<uint32_t>(nodes.size());
        nodes.emplace_back();

        // Compute bounds for this range.
        auto range_bounds = kdop_type::empty();
        for (uint32_t i = begin; i < end; ++i) {
            range_bounds.merge_in_place(leaf_bounds[prim_indices[i]]);
        }

        // Leaf case.
        if (count <= max_leaf_size) {
            nodes[node_idx].bounds = range_bounds;
            nodes[node_idx].second_child_or_primitive_offset = begin;
            nodes[node_idx].primitive_count = static_cast<uint16_t>(count);
            nodes[node_idx].split_axis = 0;
            return node_idx;
        }

        // Choose axis with longest centroid extent.
        auto centroid_bounds = kdop_type::empty();
        for (uint32_t i = begin; i < end; ++i) {
            centroid_bounds.expand(
                std::span<const T, SpatialDim>(centroids[prim_indices[i]]));
        }

        int8_t best_axis = 0;
        T best_extent = T(0);
        for (int8_t a = 0; a < SpatialDim; ++a) {
            T extent = centroid_bounds.width(a);
            if (extent > best_extent) {
                best_extent = extent;
                best_axis = a;
            }
        }

        // Split at the median.
        uint32_t mid = begin + count / 2;
        std::nth_element(prim_indices.begin() + begin,
                         prim_indices.begin() + mid,
                         prim_indices.begin() + end,
                         [&](uint32_t a, uint32_t b) {
                             return centroids[a][best_axis]
                                    < centroids[b][best_axis];
                         });

        // Fill internal node.
        nodes[node_idx].bounds = range_bounds;
        nodes[node_idx].primitive_count = 0;
        nodes[node_idx].split_axis = static_cast<uint8_t>(best_axis);

        // Left child is always at node_idx + 1 (DFS pre-order).
        build_recursive(begin, mid);

        // Right child — record its index.
        uint32_t right_idx = build_recursive(mid, end);
        nodes[node_idx].second_child_or_primitive_offset = right_idx;

        return node_idx;
    }
};

} // namespace balsa::geometry::spatial::detail
#endif
