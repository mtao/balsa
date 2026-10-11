#if !defined(BALSA_GEOMETRY_SPATIAL_DETAIL_SPATIALMEDIANBUILDER_HPP)
#define BALSA_GEOMETRY_SPATIAL_DETAIL_SPATIALMEDIANBUILDER_HPP

/// @file detail/SpatialMedianBuilder.hpp
/// @brief Spatial median BVH construction.
///
/// Splits at the spatial midpoint of the bounding box along the
/// longest axis.  Falls back to object median when one side is empty
/// (degenerate case).

#include <algorithm>
#include <cstdint>
#include <span>
#include <vector>

#include <balsa/geometry/spatial/detail/BVHNode.hpp>
#include <balsa/parallel.hpp>

namespace balsa::geometry::spatial::detail {

template <typename T, int8_t SpatialDim, int8_t K, typename DirectionPolicy>
struct SpatialMedianBuilder {
    static_assert(field<T>,
                  "SpatialMedianBuilder requires field<T> "
                  "(spatial midpoint split truncates with integral types)");

    using node_type = BVHNode<T, SpatialDim, K, DirectionPolicy>;
    using kdop_type = typename node_type::kdop_type;

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

        // Choose axis with longest spatial extent.
        int8_t best_axis = 0;
        T best_extent = T(0);
        for (int8_t a = 0; a < SpatialDim; ++a) {
            T extent = range_bounds.width(a);
            if (extent > best_extent) {
                best_extent = extent;
                best_axis = a;
            }
        }

        // Split at the spatial midpoint.
        T split_value =
            T(0.5)
            * (range_bounds.min(best_axis) + range_bounds.max(best_axis));

        auto partition_it =
            std::partition(prim_indices.begin() + begin,
                           prim_indices.begin() + end,
                           [&](uint32_t pi) {
                               return centroids[pi][best_axis] < split_value;
                           });

        uint32_t mid =
            static_cast<uint32_t>(partition_it - prim_indices.begin());

        // Degenerate case: all primitives on one side.
        // Fall back to object median.
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

        // Left child at node_idx + 1.
        build_recursive(begin, mid);

        // Right child.
        uint32_t right_idx = build_recursive(mid, end);
        nodes[node_idx].second_child_or_primitive_offset = right_idx;

        return node_idx;
    }
};

} // namespace balsa::geometry::spatial::detail
#endif
