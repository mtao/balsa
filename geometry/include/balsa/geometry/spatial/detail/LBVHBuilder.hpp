#if !defined(BALSA_GEOMETRY_SPATIAL_DETAIL_LBVHBUILDER_HPP)
#define BALSA_GEOMETRY_SPATIAL_DETAIL_LBVHBUILDER_HPP

/// @file detail/LBVHBuilder.hpp
/// @brief Linear BVH construction via Morton codes.
///
/// Computes Morton codes from primitive centroids, sorts by Morton
/// code, and builds a binary tree by splitting at the highest
/// differing bit between adjacent codes.
///
/// Reference: Karras (2012), "Maximizing Parallelism in the
/// Construction of BVHs, Octrees, and k-d Trees".

#include <algorithm>
#include <cstdint>
#include <numeric>
#include <span>
#include <vector>

#include <balsa/geometry/spatial/detail/BVHNode.hpp>
#include <balsa/parallel.hpp>

namespace balsa::geometry::spatial::detail {

/// Expand a 10-bit integer to 30 bits by inserting 2 zero bits after
/// each bit (for 3D Morton codes).
constexpr auto expand_bits_3d(uint32_t v) -> uint32_t {
    v = (v | (v << 16)) & 0x030000FFu;
    v = (v | (v << 8)) & 0x0300F00Fu;
    v = (v | (v << 4)) & 0x030C30C3u;
    v = (v | (v << 2)) & 0x09249249u;
    return v;
}

/// Expand a 16-bit integer to 32 bits by inserting 1 zero bit after
/// each bit (for 2D Morton codes).
constexpr auto expand_bits_2d(uint32_t v) -> uint32_t {
    v = (v | (v << 8)) & 0x00FF00FFu;
    v = (v | (v << 4)) & 0x0F0F0F0Fu;
    v = (v | (v << 2)) & 0x33333333u;
    v = (v | (v << 1)) & 0x55555555u;
    return v;
}

/// Compute a 30-bit Morton code for a 3D point normalized to [0,1]^3.
template <typename T>
constexpr auto morton_3d(T x, T y, T z) -> uint32_t {
    auto clamp01 = [](T v) -> T {
        return v < T(0) ? T(0) : (v > T(1) ? T(1) : v);
    };
    uint32_t xi = static_cast<uint32_t>(clamp01(x) * T(1023));
    uint32_t yi = static_cast<uint32_t>(clamp01(y) * T(1023));
    uint32_t zi = static_cast<uint32_t>(clamp01(z) * T(1023));
    return expand_bits_3d(xi) | (expand_bits_3d(yi) << 1)
           | (expand_bits_3d(zi) << 2);
}

/// Compute a 32-bit Morton code for a 2D point normalized to [0,1]^2.
template <typename T>
constexpr auto morton_2d(T x, T y) -> uint32_t {
    auto clamp01 = [](T v) -> T {
        return v < T(0) ? T(0) : (v > T(1) ? T(1) : v);
    };
    uint32_t xi = static_cast<uint32_t>(clamp01(x) * T(65535));
    uint32_t yi = static_cast<uint32_t>(clamp01(y) * T(65535));
    return expand_bits_2d(xi) | (expand_bits_2d(yi) << 1);
}

template <typename T, int8_t SpatialDim, int8_t K, typename DirectionPolicy>
struct LBVHBuilder {
    static_assert(field<T>,
                  "LBVHBuilder requires field<T> "
                  "(Morton code normalization involves division)");

    using node_type = BVHNode<T, SpatialDim, K, DirectionPolicy>;
    using kdop_type = typename node_type::kdop_type;

    std::span<const kdop_type> leaf_bounds;
    std::vector<uint32_t> &prim_indices;
    std::vector<node_type> &nodes;
    uint16_t max_leaf_size;

    std::vector<std::array<T, SpatialDim>> centroids;
    std::vector<uint32_t> morton_codes;

    void precompute_centroids() {
        centroids.resize(leaf_bounds.size());
        parallel::for_each(size_t{0}, leaf_bounds.size(), [&](size_t i) {
            centroids[i] = leaf_bounds[i].center();
        });
    }

    void compute_morton_codes() {
        // Compute scene centroid bounds for normalization.
        auto scene_bounds = kdop_type::empty();
        for (size_t i = 0; i < centroids.size(); ++i) {
            scene_bounds.expand(std::span<const T, SpatialDim>(centroids[i]));
        }

        morton_codes.resize(centroids.size());
        parallel::for_each(size_t{0}, centroids.size(), [&](size_t i) {
            std::array<T, SpatialDim> normalized{};
            for (int8_t d = 0; d < SpatialDim; ++d) {
                auto extent = scene_bounds.width(d);
                if (extent > T(0)) {
                    normalized[d] =
                        (centroids[i][d] - scene_bounds.min(d)) / extent;
                } else {
                    normalized[d] = T(0.5);
                }
            }

            if constexpr (SpatialDim == 2) {
                morton_codes[i] = morton_2d(normalized[0], normalized[1]);
            } else if constexpr (SpatialDim == 3) {
                morton_codes[i] =
                    morton_3d(normalized[0], normalized[1], normalized[2]);
            } else {
                // Fallback: use only the first 2 or 3 dimensions.
                if constexpr (SpatialDim >= 3) {
                    morton_codes[i] =
                        morton_3d(normalized[0], normalized[1], normalized[2]);
                } else {
                    morton_codes[i] = morton_2d(normalized[0], normalized[1]);
                }
            }
        });
    }

    /// Build recursively on the Morton-sorted range [begin, end).
    /// At each level, find the highest bit where codes differ and
    /// split there.
    auto build_recursive(uint32_t begin, uint32_t end) -> uint32_t {
        uint32_t count = end - begin;
        uint32_t node_idx = static_cast<uint32_t>(nodes.size());
        nodes.emplace_back();

        // Compute bounds.
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

        // Find the highest bit that differs between first and last
        // Morton codes in this range.
        uint32_t first_code = morton_codes[prim_indices[begin]];
        uint32_t last_code = morton_codes[prim_indices[end - 1]];
        uint32_t diff = first_code ^ last_code;

        uint32_t mid;
        uint8_t split_axis = 0;

        if (diff == 0) {
            // All Morton codes identical — split at midpoint.
            mid = begin + count / 2;
        } else {
            // Find highest differing bit.
            int highest_bit = 31 - __builtin_clz(diff);

            // The split axis is determined by which coordinate the highest
            // bit belongs to (bit interleaving pattern).
            if constexpr (SpatialDim == 2) {
                split_axis = static_cast<uint8_t>(highest_bit % 2);
            } else if constexpr (SpatialDim == 3) {
                // Morton interleaving: bit 0 = x, bit 1 = y, bit 2 = z,
                // bit 3 = x, ...  But we shifted: x at positions 0,3,6,...
                // y at 1,4,7,...  z at 2,5,8,...
                split_axis = static_cast<uint8_t>(highest_bit % SpatialDim);
            } else {
                split_axis = static_cast<uint8_t>(highest_bit % SpatialDim);
            }

            // Binary search for the split point: find where the highest
            // differing bit transitions from 0 to 1.
            uint32_t mask = 1u << highest_bit;
            // We want the first index where (morton_code & mask) != 0.
            auto it = std::partition_point(
                prim_indices.begin() + begin,
                prim_indices.begin() + end,
                [&](uint32_t pi) { return (morton_codes[pi] & mask) == 0; });

            mid = static_cast<uint32_t>(it - prim_indices.begin());

            // Safety: if partition_point lands at begin or end, force
            // midpoint.
            if (mid == begin || mid == end) { mid = begin + count / 2; }
        }

        // Fill internal node.
        nodes[node_idx].bounds = range_bounds;
        nodes[node_idx].primitive_count = 0;
        nodes[node_idx].split_axis = split_axis;

        // Left child.
        build_recursive(begin, mid);

        // Right child.
        uint32_t right_idx = build_recursive(mid, end);
        nodes[node_idx].second_child_or_primitive_offset = right_idx;

        return node_idx;
    }
};

} // namespace balsa::geometry::spatial::detail
#endif
