#if !defined(BALSA_GEOMETRY_SIMPLEX_POINT_IN_CIRCUMCIRCLE_HPP)
#define BALSA_GEOMETRY_SIMPLEX_POINT_IN_CIRCUMCIRCLE_HPP

// Point-in-circumsphere test.
//
// Delegates to quiver::simplex for the canonical implementation.
// Eigen overloads convert to zipper and forward.

#include <quiver/simplex/point_in_circumcircle.hpp>

#include "balsa/eigen/concepts/matrix_types.hpp"
#include "balsa/eigen/zipper_compat.hpp"

namespace balsa::geometry::simplex {

// ── Zipper overloads (delegate to quiver) ──────────────────────────────

/// Test whether point P lies inside the circumsphere of the simplex
/// whose vertices are the columns of S.
template<::zipper::concepts::Vector PointType, ::zipper::concepts::Matrix SimplexVertices>
bool point_in_circumsphere(const PointType &P, const SimplexVertices &S) {
    return ::quiver::simplex::point_in_circumsphere(P, S);
}

/// @deprecated Use point_in_circumsphere(P, S) instead.
template<::zipper::concepts::Matrix SimplexVertices, ::zipper::concepts::Vector PointType>
[[deprecated("use point_in_circumsphere(P, S) instead -- note swapped argument order")]]
bool point_in_circumcircle(const SimplexVertices &S, const PointType &P) {
    return ::quiver::simplex::point_in_circumsphere(P, S);
}

// ── Eigen overloads (convert to zipper, then delegate) ─────────────────

template<eigen::concepts::MatrixBaseDerived PointType, eigen::concepts::MatrixBaseDerived SimplexVertices>
bool point_in_circumsphere(const PointType &P, const SimplexVertices &S) {
    return ::quiver::simplex::point_in_circumsphere(eigen::as_zipper(P), eigen::as_zipper(S));
}

template<eigen::concepts::MatrixBaseDerived SimplexVertices, eigen::concepts::MatrixBaseDerived PointType>
[[deprecated("use point_in_circumsphere(P, S) instead -- note swapped argument order")]]
bool point_in_circumcircle(const SimplexVertices &S, const PointType &P) {
    return ::quiver::simplex::point_in_circumsphere(eigen::as_zipper(P), eigen::as_zipper(S));
}

}// namespace balsa::geometry::simplex

#endif
