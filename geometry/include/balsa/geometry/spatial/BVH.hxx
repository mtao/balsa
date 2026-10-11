#if !defined(BALSA_GEOMETRY_SPATIAL_BVH_HXX)
#define BALSA_GEOMETRY_SPATIAL_BVH_HXX
#include "BVH.hpp"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <stack>

#include <balsa/geometry/spatial/detail/LBVHBuilder.hpp>
#include <balsa/geometry/spatial/detail/ObjectMedianBuilder.hpp>
#include <balsa/geometry/spatial/detail/SAHBuilder.hpp>
#include <balsa/geometry/spatial/detail/SpatialMedianBuilder.hpp>
#include <balsa/parallel.hpp>

namespace balsa::geometry::spatial {

// ============================================================================
// build()
// ============================================================================

template <typename T, int8_t SpatialDim, int8_t K, typename DirectionPolicy>
void BVH<T, SpatialDim, K, DirectionPolicy>::build(
    std::span<const kdop_type> bounds,
    const BVHConfig &config) {
    m_config = config;
    m_nodes.clear();
    m_prim_indices.clear();

    if (bounds.empty()) {
        m_leaf_bounds.clear();
        return;
    }

    // Store a copy of the leaf bounds for per-primitive checks in queries.
    m_leaf_bounds.assign(bounds.begin(), bounds.end());

    auto n = static_cast<uint32_t>(bounds.size());
    m_prim_indices.resize(n);
    std::ranges::iota(m_prim_indices, uint32_t{0});

    // Reserve estimated node count (2*n - 1 for a full binary tree).
    m_nodes.reserve(2 * n);

    switch (config.strategy) {
    case BVHBuildStrategy::sah: {
        detail::SAHBuilder<T, SpatialDim, K, DirectionPolicy> builder{
            bounds, m_prim_indices, m_nodes, config.max_leaf_size, {}};
        builder.precompute_centroids();
        builder.build_recursive(0, n);
        break;
    }
    case BVHBuildStrategy::object_median: {
        detail::ObjectMedianBuilder<T, SpatialDim, K, DirectionPolicy> builder{
            bounds, m_prim_indices, m_nodes, config.max_leaf_size, {}};
        builder.precompute_centroids();
        builder.build_recursive(0, n);
        break;
    }
    case BVHBuildStrategy::spatial_median: {
        detail::SpatialMedianBuilder<T, SpatialDim, K, DirectionPolicy> builder{
            bounds, m_prim_indices, m_nodes, config.max_leaf_size, {}};
        builder.precompute_centroids();
        builder.build_recursive(0, n);
        break;
    }
    case BVHBuildStrategy::lbvh: {
        detail::LBVHBuilder<T, SpatialDim, K, DirectionPolicy> builder{
            bounds, m_prim_indices, m_nodes, config.max_leaf_size, {}, {}};
        builder.precompute_centroids();
        builder.compute_morton_codes();
        // Sort primitive indices by Morton code.
        parallel::sort(m_prim_indices.begin(),
                       m_prim_indices.end(),
                       [&](uint32_t a, uint32_t b) -> auto {
                           return builder.morton_codes[a]
                                  < builder.morton_codes[b];
                       });
        builder.build_recursive(0, n);
        break;
    }
    }
}

// ============================================================================
// rebuild()
// ============================================================================

template <typename T, int8_t SpatialDim, int8_t K, typename DirectionPolicy>
void BVH<T, SpatialDim, K, DirectionPolicy>::rebuild(
    std::span<const kdop_type> bounds) {
    build(bounds, m_config);
}

// ============================================================================
// query_overlap()
// ============================================================================

template <typename T, int8_t SpatialDim, int8_t K, typename DirectionPolicy>
void BVH<T, SpatialDim, K, DirectionPolicy>::query_overlap(
    const kdop_type &query_bounds,
    const std::function<void(uint32_t)> &callback) const {
    if (m_nodes.empty()) return;

    // Stack-based DFS traversal.
    std::stack<uint32_t> stack;
    stack.push(0);

    while (!stack.empty()) {
        uint32_t idx = stack.top();
        stack.pop();

        const auto &node = m_nodes[idx];

        // Skip if no overlap with this node's bounds.
        if (!node.bounds.overlaps(query_bounds)) continue;

        if (node.is_leaf()) {
            // Report primitives whose individual bounds overlap the query.
            uint32_t offset = node.primitive_offset();
            for (uint16_t i = 0; i < node.primitive_count; ++i) {
                uint32_t prim = m_prim_indices[offset + i];
                if (m_leaf_bounds[prim].overlaps(query_bounds)) {
                    callback(prim);
                }
            }
        } else {
            // Push right child first (so left is processed first — DFS order).
            stack.push(node.right_child());
            stack.push(idx + 1); // Left child is always at idx + 1.
        }
    }
}

// ============================================================================
// query_point()
// ============================================================================

template <typename T, int8_t SpatialDim, int8_t K, typename DirectionPolicy>
void BVH<T, SpatialDim, K, DirectionPolicy>::query_point(
    std::span<const T, SpatialDim> point,
    const std::function<void(uint32_t)> &callback) const {
    if (m_nodes.empty()) return;

    std::stack<uint32_t> stack;
    stack.push(0);

    while (!stack.empty()) {
        uint32_t idx = stack.top();
        stack.pop();

        const auto &node = m_nodes[idx];

        if (!node.bounds.contains(point)) continue;

        if (node.is_leaf()) {
            uint32_t offset = node.primitive_offset();
            for (uint16_t i = 0; i < node.primitive_count; ++i) {
                uint32_t prim = m_prim_indices[offset + i];
                if (m_leaf_bounds[prim].contains(point)) { callback(prim); }
            }
        } else {
            stack.push(node.right_child());
            stack.push(idx + 1);
        }
    }
}

// ============================================================================
// query_nearest()
// ============================================================================

template <typename T, int8_t SpatialDim, int8_t K, typename DirectionPolicy>
auto BVH<T, SpatialDim, K, DirectionPolicy>::query_nearest(
    std::span<const T, SpatialDim> point,
    const std::function<T(uint32_t)> &distance_func) const
    -> std::pair<uint32_t, T> {
    if (m_nodes.empty()) {
        return {static_cast<uint32_t>(-1), std::numeric_limits<T>::infinity()};
    }

    uint32_t best_idx = static_cast<uint32_t>(-1);
    T best_dist_sq = std::numeric_limits<T>::infinity();

    // Priority-style DFS: always visit the closer child first.
    std::stack<uint32_t> stack;
    stack.push(0);

    while (!stack.empty()) {
        uint32_t idx = stack.top();
        stack.pop();

        const auto &node = m_nodes[idx];

        // Prune: if the minimum distance from the point to this node's
        // bounding volume is already >= best, skip.
        T node_dist_sq = node.bounds.min_distance_sq(point);
        if (node_dist_sq >= best_dist_sq) continue;

        if (node.is_leaf()) {
            uint32_t offset = node.primitive_offset();
            for (uint16_t i = 0; i < node.primitive_count; ++i) {
                uint32_t prim = m_prim_indices[offset + i];
                T d = distance_func(prim);
                if (d < best_dist_sq) {
                    best_dist_sq = d;
                    best_idx = prim;
                }
            }
        } else {
            // Compute distances to both children and visit closer one first.
            uint32_t left_idx = idx + 1;
            uint32_t right_idx = node.right_child();

            T left_dist = m_nodes[left_idx].bounds.min_distance_sq(point);
            T right_dist = m_nodes[right_idx].bounds.min_distance_sq(point);

            // Push farther child first (it will be processed later).
            if (left_dist < right_dist) {
                if (right_dist < best_dist_sq) stack.push(right_idx);
                if (left_dist < best_dist_sq) stack.push(left_idx);
            } else {
                if (left_dist < best_dist_sq) stack.push(left_idx);
                if (right_dist < best_dist_sq) stack.push(right_idx);
            }
        }
    }

    return {best_idx, best_dist_sq};
}

// ============================================================================
// query_ray()
// ============================================================================

template <typename T, int8_t SpatialDim, int8_t K, typename DirectionPolicy>
void BVH<T, SpatialDim, K, DirectionPolicy>::query_ray(
    std::span<const T, SpatialDim> origin,
    std::span<const T, SpatialDim> direction,
    const std::function<std::optional<T>(uint32_t)> &hit_func,
    const std::function<void(uint32_t, T)> &callback) const {
    if (m_nodes.empty()) return;

    // Precompute direction signs for front-to-back traversal ordering.
    std::array<bool, SpatialDim> dir_positive;
    for (int8_t i = 0; i < SpatialDim; ++i) {
        dir_positive[i] = direction[i] >= T(0);
    }

    std::stack<uint32_t> stack;
    stack.push(0);

    while (!stack.empty()) {
        uint32_t idx = stack.top();
        stack.pop();

        const auto &node = m_nodes[idx];

        // Test ray against node bounds.
        auto interval = node.bounds.ray_intersect(origin, direction);
        if (!interval.has_value()) continue;

        if (node.is_leaf()) {
            uint32_t offset = node.primitive_offset();
            for (uint16_t i = 0; i < node.primitive_count; ++i) {
                uint32_t prim = m_prim_indices[offset + i];
                auto t = hit_func(prim);
                if (t.has_value()) { callback(prim, *t); }
            }
        } else {
            uint32_t left_idx = idx + 1;
            uint32_t right_idx = node.right_child();

            // Use split_axis to determine front-to-back order.
            // Push the farther child first so the nearer child is visited
            // first.
            uint8_t axis = node.split_axis;
            if (axis < SpatialDim && dir_positive[axis]) {
                // Ray goes in positive direction along split axis:
                // left child is nearer.
                stack.push(right_idx);
                stack.push(left_idx);
            } else {
                stack.push(left_idx);
                stack.push(right_idx);
            }
        }
    }
}

// ============================================================================
// query_ray_nearest()
// ============================================================================

template <typename T, int8_t SpatialDim, int8_t K, typename DirectionPolicy>
auto BVH<T, SpatialDim, K, DirectionPolicy>::query_ray_nearest(
    std::span<const T, SpatialDim> origin,
    std::span<const T, SpatialDim> direction,
    const std::function<std::optional<T>(uint32_t)> &hit_func) const
    -> std::pair<uint32_t, T> {
    if (m_nodes.empty()) {
        return {static_cast<uint32_t>(-1), std::numeric_limits<T>::infinity()};
    }

    uint32_t best_idx = static_cast<uint32_t>(-1);
    T best_t = std::numeric_limits<T>::infinity();

    std::array<bool, SpatialDim> dir_positive;
    for (int8_t i = 0; i < SpatialDim; ++i) {
        dir_positive[i] = direction[i] >= T(0);
    }

    std::stack<uint32_t> stack;
    stack.push(0);

    while (!stack.empty()) {
        uint32_t idx = stack.top();
        stack.pop();

        const auto &node = m_nodes[idx];

        auto interval = node.bounds.ray_intersect(origin, direction);
        if (!interval.has_value()) continue;

        // Early termination: if the nearest point of entry is already
        // beyond the best hit, skip this subtree.
        if (interval->first >= best_t) continue;

        if (node.is_leaf()) {
            uint32_t offset = node.primitive_offset();
            for (uint16_t i = 0; i < node.primitive_count; ++i) {
                uint32_t prim = m_prim_indices[offset + i];
                auto t = hit_func(prim);
                if (t.has_value() && *t >= T(0) && *t < best_t) {
                    best_t = *t;
                    best_idx = prim;
                }
            }
        } else {
            uint32_t left_idx = idx + 1;
            uint32_t right_idx = node.right_child();

            uint8_t axis = node.split_axis;
            if (axis < SpatialDim && dir_positive[axis]) {
                stack.push(right_idx);
                stack.push(left_idx);
            } else {
                stack.push(left_idx);
                stack.push(right_idx);
            }
        }
    }

    return {best_idx, best_t};
}

// ============================================================================
// query_within_radius()
// ============================================================================

template <typename T, int8_t SpatialDim, int8_t K, typename DirectionPolicy>
void BVH<T, SpatialDim, K, DirectionPolicy>::query_within_radius(
    std::span<const T, SpatialDim> point,
    T radius_sq,
    const std::function<T(uint32_t)> &distance_func,
    const std::function<void(uint32_t, T)> &callback) const {
    if (m_nodes.empty()) return;

    std::stack<uint32_t> stack;
    stack.push(0);

    while (!stack.empty()) {
        uint32_t idx = stack.top();
        stack.pop();

        const auto &node = m_nodes[idx];

        // Prune: if BV minimum distance exceeds the radius, skip.
        if (node.bounds.min_distance_sq(point) > radius_sq) continue;

        if (node.is_leaf()) {
            uint32_t offset = node.primitive_offset();
            for (uint16_t i = 0; i < node.primitive_count; ++i) {
                uint32_t prim = m_prim_indices[offset + i];
                T d = distance_func(prim);
                if (d <= radius_sq) { callback(prim, d); }
            }
        } else {
            stack.push(node.right_child());
            stack.push(idx + 1);
        }
    }
}

// ============================================================================
// query_k_nearest()
// ============================================================================

template <typename T, int8_t SpatialDim, int8_t K, typename DirectionPolicy>
auto BVH<T, SpatialDim, K, DirectionPolicy>::query_k_nearest(
    std::span<const T, SpatialDim> point,
    uint32_t k,
    const std::function<T(uint32_t)> &distance_func) const
    -> std::vector<std::pair<uint32_t, T>> {
    if (m_nodes.empty() || k == 0) { return {}; }

    // Max-heap: top element is the farthest of the k-nearest so far.
    // Pair: (distance_sq, primitive_index).
    auto cmp = [](const std::pair<T, uint32_t> &a,
                  const std::pair<T, uint32_t> &b) {
        return a.first < b.first;
    };
    std::vector<std::pair<T, uint32_t>> heap;
    heap.reserve(k);

    T threshold = std::numeric_limits<T>::infinity();

    std::stack<uint32_t> stack;
    stack.push(0);

    while (!stack.empty()) {
        uint32_t idx = stack.top();
        stack.pop();

        const auto &node = m_nodes[idx];

        if (node.bounds.min_distance_sq(point) >= threshold) continue;

        if (node.is_leaf()) {
            uint32_t offset = node.primitive_offset();
            for (uint16_t i = 0; i < node.primitive_count; ++i) {
                uint32_t prim = m_prim_indices[offset + i];
                T d = distance_func(prim);
                if (heap.size() < k) {
                    heap.emplace_back(d, prim);
                    std::push_heap(heap.begin(), heap.end(), cmp);
                    if (heap.size() == k) { threshold = heap.front().first; }
                } else if (d < threshold) {
                    std::pop_heap(heap.begin(), heap.end(), cmp);
                    heap.back() = {d, prim};
                    std::push_heap(heap.begin(), heap.end(), cmp);
                    threshold = heap.front().first;
                }
            }
        } else {
            // Visit closer child first for better pruning.
            uint32_t left_idx = idx + 1;
            uint32_t right_idx = node.right_child();

            T left_dist = m_nodes[left_idx].bounds.min_distance_sq(point);
            T right_dist = m_nodes[right_idx].bounds.min_distance_sq(point);

            if (left_dist < right_dist) {
                if (right_dist < threshold) stack.push(right_idx);
                if (left_dist < threshold) stack.push(left_idx);
            } else {
                if (left_dist < threshold) stack.push(left_idx);
                if (right_dist < threshold) stack.push(right_idx);
            }
        }
    }

    // Convert heap to sorted result vector: (index, distance_sq).
    std::sort(heap.begin(), heap.end(), cmp);
    std::vector<std::pair<uint32_t, T>> result;
    result.reserve(heap.size());
    for (auto &[d, idx] : heap) { result.emplace_back(idx, d); }
    return result;
}

// ============================================================================
// query_overlap_bvh()
// ============================================================================

template <typename T, int8_t SpatialDim, int8_t K, typename DirectionPolicy>
void BVH<T, SpatialDim, K, DirectionPolicy>::query_overlap_bvh(
    const BVH &other,
    const std::function<void(uint32_t, uint32_t)> &callback) const {
    if (m_nodes.empty() || other.m_nodes.empty()) return;

    // Dual-tree descent with (node_a, node_b) pairs on the stack.
    using NodePair = std::pair<uint32_t, uint32_t>;
    std::stack<NodePair> stack;
    stack.push({0, 0});

    while (!stack.empty()) {
        auto [idx_a, idx_b] = stack.top();
        stack.pop();

        const auto &node_a = m_nodes[idx_a];
        const auto &node_b = other.m_nodes[idx_b];

        // Test overlap between the two node bounds.
        if (!node_a.bounds.overlaps(node_b.bounds)) continue;

        if (node_a.is_leaf() && node_b.is_leaf()) {
            // Both leaves — test all primitive pairs.
            uint32_t off_a = node_a.primitive_offset();
            uint32_t off_b = node_b.primitive_offset();
            for (uint16_t i = 0; i < node_a.primitive_count; ++i) {
                uint32_t prim_a = m_prim_indices[off_a + i];
                for (uint16_t j = 0; j < node_b.primitive_count; ++j) {
                    uint32_t prim_b = other.m_prim_indices[off_b + j];
                    if (m_leaf_bounds[prim_a].overlaps(
                            other.m_leaf_bounds[prim_b])) {
                        callback(prim_a, prim_b);
                    }
                }
            }
        } else if (node_b.is_leaf()
                   || (!node_a.is_leaf()
                       && node_a.bounds.volume() >= node_b.bounds.volume())) {
            // Split node_a (it has larger volume or node_b is a leaf).
            uint32_t left_a = idx_a + 1;
            uint32_t right_a = node_a.right_child();
            stack.push({left_a, idx_b});
            stack.push({right_a, idx_b});
        } else {
            // Split node_b.
            uint32_t left_b = idx_b + 1;
            uint32_t right_b = node_b.right_child();
            stack.push({idx_a, left_b});
            stack.push({idx_a, right_b});
        }
    }
}

// ============================================================================
// refit()
// ============================================================================

template <typename T, int8_t SpatialDim, int8_t K, typename DirectionPolicy>
void BVH<T, SpatialDim, K, DirectionPolicy>::refit(
    std::span<const kdop_type> bounds) {
    if (m_nodes.empty()) return;

    // Update stored leaf bounds for per-primitive query checks.
    m_leaf_bounds.assign(bounds.begin(), bounds.end());

    // Reverse-order bottom-up: since nodes are in DFS pre-order, a
    // reverse traversal guarantees children are processed before parents.
    for (int64_t i = static_cast<int64_t>(m_nodes.size()) - 1; i >= 0; --i) {
        auto &node = m_nodes[static_cast<size_t>(i)];
        if (node.is_leaf()) {
            // Recompute leaf bounds from current primitive bounds.
            node.bounds = kdop_type::empty();
            uint32_t offset = node.primitive_offset();
            for (uint16_t j = 0; j < node.primitive_count; ++j) {
                node.bounds.merge_in_place(bounds[m_prim_indices[offset + j]]);
            }
        } else {
            // Internal node: merge children's bounds.
            uint32_t left_idx = static_cast<uint32_t>(i) + 1;
            uint32_t right_idx = node.right_child();
            node.bounds =
                m_nodes[left_idx].bounds.merge(m_nodes[right_idx].bounds);
        }
    }
}

// ============================================================================
// insert()
// ============================================================================

template <typename T, int8_t SpatialDim, int8_t K, typename DirectionPolicy>
void BVH<T, SpatialDim, K, DirectionPolicy>::insert(uint32_t index,
                                                    const kdop_type &bounds) {
    if (m_nodes.empty()) {
        // First primitive — create a single leaf.
        // Ensure m_leaf_bounds is large enough.
        if (index >= m_leaf_bounds.size()) {
            m_leaf_bounds.resize(index + 1, kdop_type::empty());
        }
        m_leaf_bounds[index] = bounds;
        m_prim_indices.push_back(index);
        node_type leaf;
        leaf.bounds = bounds;
        leaf.second_child_or_primitive_offset = 0;
        leaf.primitive_count = 1;
        leaf.split_axis = 0;
        m_nodes.push_back(leaf);
        return;
    }

    // Store the new primitive's bounds.
    if (index >= m_leaf_bounds.size()) {
        m_leaf_bounds.resize(index + 1, kdop_type::empty());
    }
    m_leaf_bounds[index] = bounds;

    // Find the best leaf to insert into.  Walk from root, always
    // choosing the child whose bounds need less expansion (by surface
    // area increase).
    uint32_t node_idx = 0;
    while (!m_nodes[node_idx].is_leaf()) {
        auto &node = m_nodes[node_idx];
        node.bounds.merge_in_place(bounds);

        uint32_t left_idx = node_idx + 1;
        uint32_t right_idx = node.right_child();

        auto left_merged = m_nodes[left_idx].bounds.merge(bounds);
        auto right_merged = m_nodes[right_idx].bounds.merge(bounds);

        auto left_cost = left_merged.surface_area()
                         - m_nodes[left_idx].bounds.surface_area();
        auto right_cost = right_merged.surface_area()
                          - m_nodes[right_idx].bounds.surface_area();

        node_idx = (left_cost <= right_cost) ? left_idx : right_idx;
    }

    // Found a leaf node.  If it has room, just add the primitive.
    auto &leaf = m_nodes[node_idx];
    if (leaf.primitive_count < m_config.max_leaf_size) {
        // Append to the end of the primitive index array.
        // We need to move this leaf's primitives to the end if they
        // aren't already there.
        uint32_t new_offset = static_cast<uint32_t>(m_prim_indices.size());
        uint32_t old_offset = leaf.primitive_offset();
        for (uint16_t i = 0; i < leaf.primitive_count; ++i) {
            m_prim_indices.push_back(m_prim_indices[old_offset + i]);
        }
        m_prim_indices.push_back(index);
        leaf.second_child_or_primitive_offset = new_offset;
        leaf.primitive_count++;
        leaf.bounds.merge_in_place(bounds);
    } else {
        // Leaf is full — need to split.  For simplicity, just mark this
        // as needing a rebuild.  A full incremental split would require
        // re-indexing the node array (since DFS pre-order layout means
        // inserting a node shifts all subsequent indices).
        //
        // Simple approach: append the primitive and trigger a deferred
        // quality degradation.  The user should call rebuild() when
        // tree quality deteriorates.
        uint32_t new_offset = static_cast<uint32_t>(m_prim_indices.size());
        uint32_t old_offset = leaf.primitive_offset();
        for (uint16_t i = 0; i < leaf.primitive_count; ++i) {
            m_prim_indices.push_back(m_prim_indices[old_offset + i]);
        }
        m_prim_indices.push_back(index);
        leaf.second_child_or_primitive_offset = new_offset;
        leaf.primitive_count++;
        leaf.bounds.merge_in_place(bounds);
    }
}

// ============================================================================
// remove()
// ============================================================================

template <typename T, int8_t SpatialDim, int8_t K, typename DirectionPolicy>
void BVH<T, SpatialDim, K, DirectionPolicy>::remove(uint32_t index) {
    if (m_nodes.empty()) return;

    // Find and remove the primitive from its leaf.
    // Walk the tree to find the leaf containing this primitive.
    std::stack<uint32_t> stack;
    stack.push(0);

    while (!stack.empty()) {
        uint32_t idx = stack.top();
        stack.pop();

        auto &node = m_nodes[idx];

        if (node.is_leaf()) {
            uint32_t offset = node.primitive_offset();
            for (uint16_t i = 0; i < node.primitive_count; ++i) {
                if (m_prim_indices[offset + i] == index) {
                    // Swap with last primitive in this leaf and decrease count.
                    m_prim_indices[offset + i] =
                        m_prim_indices[offset + node.primitive_count - 1];
                    node.primitive_count--;
                    return; // Found and removed.
                }
            }
        } else {
            // Check if the primitive could be in either child.
            // We don't have the primitive's bounds, so we must check both.
            stack.push(node.right_child());
            stack.push(idx + 1);
        }
    }
}

// ============================================================================
// Accessors
// ============================================================================

template <typename T, int8_t SpatialDim, int8_t K, typename DirectionPolicy>
auto BVH<T, SpatialDim, K, DirectionPolicy>::is_built() const -> bool {
    return !m_nodes.empty();
}

template <typename T, int8_t SpatialDim, int8_t K, typename DirectionPolicy>
auto BVH<T, SpatialDim, K, DirectionPolicy>::node_count() const -> size_t {
    return m_nodes.size();
}

template <typename T, int8_t SpatialDim, int8_t K, typename DirectionPolicy>
auto BVH<T, SpatialDim, K, DirectionPolicy>::primitive_count() const -> size_t {
    return m_prim_indices.size();
}

template <typename T, int8_t SpatialDim, int8_t K, typename DirectionPolicy>
auto BVH<T, SpatialDim, K, DirectionPolicy>::nodes() const
    -> std::span<const node_type> {
    return m_nodes;
}

template <typename T, int8_t SpatialDim, int8_t K, typename DirectionPolicy>
auto BVH<T, SpatialDim, K, DirectionPolicy>::primitive_indices() const
    -> std::span<const uint32_t> {
    return m_prim_indices;
}

template <typename T, int8_t SpatialDim, int8_t K, typename DirectionPolicy>
auto BVH<T, SpatialDim, K, DirectionPolicy>::root_bounds() const
    -> const kdop_type & {
    return m_nodes[0].bounds;
}

template <typename T, int8_t SpatialDim, int8_t K, typename DirectionPolicy>
auto BVH<T, SpatialDim, K, DirectionPolicy>::height() const -> int32_t {
    if (m_nodes.empty()) return -1;
    return height_recursive(0);
}

template <typename T, int8_t SpatialDim, int8_t K, typename DirectionPolicy>
auto BVH<T, SpatialDim, K, DirectionPolicy>::config() const
    -> const BVHConfig & {
    return m_config;
}

// ============================================================================
// height_recursive() (private)
// ============================================================================

template <typename T, int8_t SpatialDim, int8_t K, typename DirectionPolicy>
auto BVH<T, SpatialDim, K, DirectionPolicy>::height_recursive(
    uint32_t idx) const -> int32_t {
    const auto &node = m_nodes[idx];
    if (node.is_leaf()) return 0;

    int32_t left_h = height_recursive(idx + 1);
    int32_t right_h = height_recursive(node.right_child());
    return 1 + std::max(left_h, right_h);
}

} // namespace balsa::geometry::spatial
#endif
