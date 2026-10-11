#if !defined(BALSA_GEOMETRY_SPATIAL_DETAIL_BVHNODE_HPP)
#define BALSA_GEOMETRY_SPATIAL_DETAIL_BVHNODE_HPP

/// @file detail/BVHNode.hpp
/// @brief BVH node struct stored in DFS pre-order.
///
/// Nodes are laid out in depth-first pre-order.  The left child of
/// node i is always at index i + 1 (implicit).  For internal nodes
/// the right child index is stored explicitly.  Leaves store an
/// offset and count into a separate primitive index array.
///
/// Discriminant: primitive_count == 0 means internal node.

#include <cstdint>

#include <balsa/geometry/spatial/KDOP.hpp>

namespace balsa::geometry::spatial::detail {

template <typename T,
          int8_t SpatialDim,
          int8_t K,
          typename DirectionPolicy = DefaultDirections<SpatialDim, K>>
struct BVHNode {
    // ring<T> suffices: BVHNode is a plain data struct holding a KDOP
    // and tree-structure metadata.  No division-requiring operations.
    static_assert(ring<T>,
                  "BVHNode requires ring<T> (arithmetic type with +, -, *).");
    using kdop_type = KDOP<T, SpatialDim, K, DirectionPolicy>;

    /// Bounding volume enclosing all primitives in this subtree.
    kdop_type bounds = kdop_type::empty();

    /// For internal nodes: index of the right child in the node array.
    /// For leaf nodes: offset into the primitive index array.
    uint32_t second_child_or_primitive_offset = 0;

    /// Number of primitives in this leaf.  0 means internal node.
    uint16_t primitive_count = 0;

    /// Split axis used at this internal node (0, 1, 2, ...).
    /// Enables ordered traversal (front-to-back by ray direction).
    /// Unused for leaves.
    uint8_t split_axis = 0;

    // -- Queries
    // ---------------------------------------------------------------

    [[nodiscard]] auto is_leaf() const -> bool { return primitive_count > 0; }

    /// Right child index (only valid for internal nodes).
    [[nodiscard]] auto right_child() const -> uint32_t {
        return second_child_or_primitive_offset;
    }

    /// Primitive offset into the index array (only valid for leaves).
    [[nodiscard]] auto primitive_offset() const -> uint32_t {
        return second_child_or_primitive_offset;
    }
};

} // namespace balsa::geometry::spatial::detail
#endif
