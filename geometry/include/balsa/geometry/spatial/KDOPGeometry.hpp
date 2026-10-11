#if !defined(BALSA_GEOMETRY_SPATIAL_KDOPGEOMETRY_HPP)
#define BALSA_GEOMETRY_SPATIAL_KDOPGEOMETRY_HPP

/// @file KDOPGeometry.hpp
/// @brief Extract explicit geometry (vertices, edges) from a k-DOP.
///
/// Converts the H-representation (halfspace intersection) of a k-DOP into
/// V-representation (vertices) plus combinatorial edge connectivity.
/// Useful for visualization and debugging of k-DOP bounding volumes.

#include <array>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

#include "KDOP.hpp"

namespace balsa::geometry::spatial {

/// Explicit geometry extracted from a k-DOP polytope.
///
/// Contains the vertices (positions) and edges (index pairs) of the
/// polytope boundary.  Vertices are deduplicated within a tolerance.
template <typename T, int8_t SpatialDim>
struct KDOPGeometry {
    /// Vertex positions on the polytope boundary.
    std::vector<std::array<T, SpatialDim>> vertices;

    /// Edges as pairs of vertex indices.
    std::vector<std::pair<size_t, size_t>> edges;
};

/// Extract the boundary geometry of a k-DOP polytope.
///
/// Enumerates all C(2K, SpatialDim) combinations of boundary planes,
/// solves for their intersection point, and retains those that satisfy
/// all slab constraints (i.e. lie on the polytope boundary).  Edges
/// connect vertex pairs that share exactly SpatialDim-1 boundary planes.
///
/// @param kdop  The k-DOP to extract geometry from.
/// @return      A KDOPGeometry struct with vertices and edges.
template <typename T, int8_t SpatialDim, int8_t K, typename DirectionPolicy>
// field<T> required: vertex enumeration solves SpatialDim × SpatialDim
// linear systems (matrix inverse) to find plane intersection points.
    requires(field<T>)
auto kdop_geometry(const KDOP<T, SpatialDim, K, DirectionPolicy> &kdop)
    -> KDOPGeometry<T, SpatialDim>;

} // namespace balsa::geometry::spatial

#include "KDOPGeometry.hxx"
#endif
