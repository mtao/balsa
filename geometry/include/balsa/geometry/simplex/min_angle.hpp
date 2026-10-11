#if !defined(BALSA_GEOMETRY_SIMPLEX_MIN_ANGLE_HPP)
#define BALSA_GEOMETRY_SIMPLEX_MIN_ANGLE_HPP

/// @file min_angle.hpp
/// @brief Minimum angle quality metrics for simplices.
///
/// Four-layer design:
///   - **Layer 1 (zipper-concepts core)**: constrained on
///   `::zipper::concepts::Vector`.
///   - **Layer 1.5 (PointSpan wrappers)**: thin overloads converting spans.
///   - **Layer 1.5M (Matrix wrappers)**: overloads accepting a D×N matrix.
///   - **Layer 1.5ID (HasAsTuple wrappers)**: for IncidentData and similar.
///
/// `min_angle(a, b, c)` — smallest interior angle of a triangle, in radians.
///   Range [0, pi/3], ideal = pi/3 (equilateral).
///
/// `min_dihedral_angle(a, b, c, d)` — smallest dihedral angle of a tet, in
/// radians.
///   Range [0, ~1.2310], ideal ~ 1.2310 (arccos(1/3), regular tet).

#include <algorithm>
#include <balsa/geometry/simplex/angle_at_vertex.hpp>
#include <balsa/geometry/simplex/concepts.hpp>
#include <balsa/geometry/simplex/detail/tet_dihedral_angles.hpp>
#include <cmath>
#include <cstddef>
#include <tuple>
#include <utility>

#include <zipper/VectorBase.hpp>
#include <zipper/concepts/Matrix.hpp>
#include <zipper/concepts/Vector.hpp>

namespace balsa::geometry::simplex {

// ============================================================================
// Layer 1: zipper-concepts core
// ============================================================================

/// Minimum interior angle of a triangle, in radians (zipper core).
///
/// In 2D, angle_at_vertex returns signed angles; we take std::abs
/// to get interior angle magnitudes before comparing.
template <::zipper::concepts::Vector V>
    requires(V::extents_type::static_extent(0) >= 2)
auto min_angle(const V &a, const V &b, const V &c) -> typename V::value_type {
    using T = typename V::value_type;

    T a0 = std::abs(angle_at_vertex(a, b, c)); // angle at A
    T a1 = std::abs(angle_at_vertex(b, c, a)); // angle at B
    T a2 = std::abs(angle_at_vertex(c, a, b)); // angle at C

    return std::min({a0, a1, a2});
}

/// Minimum dihedral angle of a tetrahedron, in radians (zipper core).
template <::zipper::concepts::Vector V>
    requires(V::extents_type::static_extent(0) == 3)
auto min_dihedral_angle(const V &a, const V &b, const V &c, const V &d) ->
    typename V::value_type {
    auto angles = detail::tet_dihedral_angles(a, b, c, d);
    return *std::min_element(angles.begin(), angles.end());
}

// ============================================================================
// Layer 1.5: PointSpan wrappers
// ============================================================================

/// Minimum interior angle of a triangle, in radians (PointSpan wrapper).
template <PointSpan S>
    requires(dim_v<S> >= 2)
auto min_angle(S a, S b, S c) -> scalar_t<S> {
    return min_angle(::zipper::VectorBase(a),
                     ::zipper::VectorBase(b),
                     ::zipper::VectorBase(c));
}

/// Minimum dihedral angle of a tetrahedron, in radians (PointSpan wrapper).
template <PointSpan S>
    requires(dim_v<S> == 3)
auto min_dihedral_angle(S a, S b, S c, S d) -> scalar_t<S> {
    return min_dihedral_angle(::zipper::VectorBase(a),
                              ::zipper::VectorBase(b),
                              ::zipper::VectorBase(c),
                              ::zipper::VectorBase(d));
}

// ============================================================================
// Layer 1.5M: Matrix wrappers
// ============================================================================

/// Minimum interior angle of a triangle, in radians, from a D×3 matrix.
template <::zipper::concepts::Matrix M>
    requires(M::extents_type::static_extent(0) >= 2)
            && (M::extents_type::static_extent(1) == 3)
auto min_angle(const M &pts) {
    return min_angle(pts.col(std::size_t{0}),
                     pts.col(std::size_t{1}),
                     pts.col(std::size_t{2}));
}

/// Minimum dihedral angle of a tetrahedron, in radians, from a 3×4 matrix.
template <::zipper::concepts::Matrix M>
    requires(M::extents_type::static_extent(0) == 3)
            && (M::extents_type::static_extent(1) == 4)
auto min_dihedral_angle(const M &pts) {
    return min_dihedral_angle(pts.col(std::size_t{0}),
                              pts.col(std::size_t{1}),
                              pts.col(std::size_t{2}),
                              pts.col(std::size_t{3}));
}

// ============================================================================
// Layer 1.5ID: HasAsTuple wrappers (IncidentData and similar)
// ============================================================================

/// Minimum interior angle of a triangle (HasAsTuple wrapper, 3 vertices).
template <HasAsTuple ID>
    requires(std::tuple_size_v<decltype(std::declval<ID>().as_tuple())> == 3)
auto min_angle(const ID &id) {
    return std::apply(
        [](const auto &...vs) {
            return min_angle(::zipper::VectorBase(vs)...);
        },
        id.as_tuple());
}

/// Minimum dihedral angle of a tet (HasAsTuple wrapper, 4 vertices).
template <HasAsTuple ID>
    requires(std::tuple_size_v<decltype(std::declval<ID>().as_tuple())> == 4)
auto min_dihedral_angle(const ID &id) {
    return std::apply(
        [](const auto &...vs) {
            return min_dihedral_angle(::zipper::VectorBase(vs)...);
        },
        id.as_tuple());
}

} // namespace balsa::geometry::simplex
#endif
