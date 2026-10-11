#if !defined(BALSA_GEOMETRY_SPATIAL_DETAIL_SAHBUILDER_HPP)
#define BALSA_GEOMETRY_SPATIAL_DETAIL_SAHBUILDER_HPP

/// @file detail/SAHBuilder.hpp
/// @brief Binned Surface Area Heuristic BVH construction.
///
/// At each node, bins primitives into B buckets along each axis and
/// evaluates the SAH cost function to find the optimal split.  Falls
/// back to a leaf when the no-split cost is lower.
///
/// Reference: Wald (2007), "On fast Construction of SAH-based
/// Bounding Volume Hierarchies".

#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <numeric>
#include <span>
#include <vector>

#include <balsa/geometry/spatial/detail/BVHNode.hpp>
#include <balsa/parallel.hpp>

namespace balsa::geometry::spatial::detail {

/// Build context for binned SAH construction.
template <typename T, int8_t SpatialDim, int8_t K, typename DirectionPolicy>
struct SAHBuilder {
    static_assert(field<T>,
                  "SAHBuilder requires field<T> (uses division for "
                  "binning and surface area heuristic evaluation)");

    using node_type = BVHNode<T, SpatialDim, K, DirectionPolicy>;
    using kdop_type = typename node_type::kdop_type;

    /// Number of bins for the SAH sweep.
    static constexpr int32_t num_bins = 16;

    /// Relative cost of traversing an internal node vs intersecting a
    /// primitive.  A value < 1 biases toward deeper trees.
    static constexpr T traversal_cost = T(1);
    static constexpr T intersect_cost = T(1);

    std::span<const kdop_type> leaf_bounds;
    std::vector<uint32_t> &prim_indices;
    std::vector<node_type> &nodes;
    uint16_t max_leaf_size;

    std::vector<std::array<T, SpatialDim>> centroids;

    void precompute_centroids() {
        centroids.resize(leaf_bounds.size());
        parallel::for_each(size_t{0}, leaf_bounds.size(), [&](size_t i) {
            centroids[i] = leaf_bounds[i].center();
        });
    }

    /// Per-bin accumulation data.
    struct Bin {
        kdop_type bounds = kdop_type::empty();
        uint32_t count = 0;
    };

    auto build_recursive(uint32_t begin, uint32_t end) -> uint32_t {
        uint32_t count = end - begin;
        uint32_t node_idx = static_cast<uint32_t>(nodes.size());
        nodes.emplace_back();

        // Compute bounds for this range.
        auto range_bounds = kdop_type::empty();
        for (uint32_t i = begin; i < end; ++i) {
            range_bounds.merge_in_place(leaf_bounds[prim_indices[i]]);
        }

        // Leaf case: small enough.
        if (count <= max_leaf_size) {
            nodes[node_idx].bounds = range_bounds;
            nodes[node_idx].second_child_or_primitive_offset = begin;
            nodes[node_idx].primitive_count = static_cast<uint16_t>(count);
            nodes[node_idx].split_axis = 0;
            return node_idx;
        }

        // Compute centroid bounds.
        auto centroid_bounds = kdop_type::empty();
        for (uint32_t i = begin; i < end; ++i) {
            centroid_bounds.expand(
                std::span<const T, SpatialDim>(centroids[prim_indices[i]]));
        }

        // Try each axis — find the best binned SAH split.
        T best_cost = std::numeric_limits<T>::max();
        int8_t best_axis = 0;
        int32_t best_split_bin = -1;

        T no_split_cost = intersect_cost * count;

        for (int8_t axis = 0; axis < SpatialDim; ++axis) {
            T axis_min = centroid_bounds.min(axis);
            T axis_max = centroid_bounds.max(axis);
            T axis_extent = axis_max - axis_min;

            if (axis_extent <= T(0))
                continue; // All centroids identical on this axis.

            // Bin primitives.
            std::array<Bin, num_bins> bins{};
            T inv_extent = T(num_bins) / axis_extent;

            for (uint32_t i = begin; i < end; ++i) {
                uint32_t pi = prim_indices[i];
                int32_t b = static_cast<int32_t>(
                    (centroids[pi][axis] - axis_min) * inv_extent);
                b = std::clamp(b, 0, num_bins - 1);
                bins[b].bounds.merge_in_place(leaf_bounds[pi]);
                bins[b].count++;
            }

            // Sweep from left: cumulative bounds and counts.
            std::array<T, num_bins - 1> left_sa{};
            std::array<uint32_t, num_bins - 1> left_count{};
            {
                auto accum = kdop_type::empty();
                uint32_t accum_count = 0;
                for (int32_t i = 0; i < num_bins - 1; ++i) {
                    accum.merge_in_place(bins[i].bounds);
                    accum_count += bins[i].count;
                    left_sa[i] = accum.surface_area();
                    left_count[i] = accum_count;
                }
            }

            // Sweep from right: cumulative bounds and counts.
            std::array<T, num_bins - 1> right_sa{};
            std::array<uint32_t, num_bins - 1> right_count{};
            {
                auto accum = kdop_type::empty();
                uint32_t accum_count = 0;
                for (int32_t i = num_bins - 1; i > 0; --i) {
                    accum.merge_in_place(bins[i].bounds);
                    accum_count += bins[i].count;
                    right_sa[i - 1] = accum.surface_area();
                    right_count[i - 1] = accum_count;
                }
            }

            // Evaluate SAH cost at each split position.
            T parent_sa = range_bounds.surface_area();
            if (parent_sa <= T(0)) continue;
            T inv_parent_sa = T(1) / parent_sa;

            for (int32_t i = 0; i < num_bins - 1; ++i) {
                if (left_count[i] == 0 || right_count[i] == 0) continue;

                T cost = traversal_cost
                         + inv_parent_sa
                               * (left_sa[i] * left_count[i]
                                  + right_sa[i] * right_count[i])
                               * intersect_cost;

                if (cost < best_cost) {
                    best_cost = cost;
                    best_axis = axis;
                    best_split_bin = i;
                }
            }
        }

        // If no valid split found, or leaf is cheaper, make a leaf.
        // (Allow up to 2*max_leaf_size in a forced leaf for degenerate cases.)
        if (best_split_bin < 0
            || (best_cost >= no_split_cost
                && count <= static_cast<uint32_t>(max_leaf_size) * 2)) {
            nodes[node_idx].bounds = range_bounds;
            nodes[node_idx].second_child_or_primitive_offset = begin;
            nodes[node_idx].primitive_count = static_cast<uint16_t>(count);
            nodes[node_idx].split_axis = 0;
            return node_idx;
        }

        // Partition primitives by the chosen bin split.
        T axis_min = centroid_bounds.min(best_axis);
        T axis_extent = centroid_bounds.max(best_axis) - axis_min;
        T inv_extent = T(num_bins) / axis_extent;

        auto partition_it = std::partition(
            prim_indices.begin() + begin,
            prim_indices.begin() + end,
            [&](uint32_t pi) {
                int32_t b = static_cast<int32_t>(
                    (centroids[pi][best_axis] - axis_min) * inv_extent);
                b = std::clamp(b, 0, num_bins - 1);
                return b <= best_split_bin;
            });

        uint32_t mid =
            static_cast<uint32_t>(partition_it - prim_indices.begin());

        // Degenerate partition — fall back to midpoint split.
        if (mid == begin || mid == end) {
            mid = begin + count / 2;
            std::nth_element(prim_indices.begin() + begin,
                             prim_indices.begin() + mid,
                             prim_indices.begin() + end,
                             [&](uint32_t a, uint32_t b) {
                                 return centroids[a][best_axis]
                                        < centroids[b][best_axis];
                             });
        }

        // Fill internal node.
        nodes[node_idx].bounds = range_bounds;
        nodes[node_idx].primitive_count = 0;
        nodes[node_idx].split_axis = static_cast<uint8_t>(best_axis);

        // Left child at node_idx + 1 (DFS pre-order).
        build_recursive(begin, mid);

        // Right child.
        uint32_t right_idx = build_recursive(mid, end);
        nodes[node_idx].second_child_or_primitive_offset = right_idx;

        return node_idx;
    }
};

} // namespace balsa::geometry::spatial::detail
#endif
