#if !defined(BALSA_GEOMETRY_SIMPLEX_POINT_IN_CIRCUMCIRCLE_HPP)
#define BALSA_GEOMETRY_SIMPLEX_POINT_IN_CIRCUMCIRCLE_HPP

// Point-in-circumsphere test.
//
// When quiver is available, delegates to quiver::simplex.  Otherwise a
// standalone implementation using the circumcenter fallback is provided.

#if defined(BALSA_HAS_QUIVER)
#include <quiver/simplex/point_in_circumcircle.hpp>
#endif

#include "balsa/geometry/simplex/circumcenter.hpp"
#include "balsa/eigen/zipper_compat.hpp"

namespace balsa::geometry::simplex {

#if defined(BALSA_HAS_QUIVER)

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

#else// !BALSA_HAS_QUIVER -- standalone implementation

template<eigen::concepts::MatrixBaseDerived PointType, eigen::concepts::MatrixBaseDerived SimplexVertices>
bool point_in_circumsphere(const PointType &P, const SimplexVertices &S) {
    auto [C, r2] = circumcenter_with_squared_radius(S);
    return (C - P).squaredNorm() < r2;
}

template<eigen::concepts::MatrixBaseDerived SimplexVertices, eigen::concepts::MatrixBaseDerived PointType>
[[deprecated("use point_in_circumsphere(P, S) instead -- note swapped argument order")]]
bool point_in_circumcircle(const SimplexVertices &S, const PointType &P) {
    return point_in_circumsphere(P, S);
}

// Zipper overloads: convert to Eigen
template<::zipper::concepts::Vector PointType, ::zipper::concepts::Matrix SimplexVertices>
bool point_in_circumsphere(const PointType &P, const SimplexVertices &S) {
    return point_in_circumsphere(eigen::as_eigen(P), eigen::as_eigen(S));
}

template<::zipper::concepts::Matrix SimplexVertices, ::zipper::concepts::Vector PointType>
[[deprecated("use point_in_circumsphere(P, S) instead -- note swapped argument order")]]
bool point_in_circumcircle(const SimplexVertices &S, const PointType &P) {
    return point_in_circumsphere(eigen::as_eigen(P), eigen::as_eigen(S));
}

#endif// BALSA_HAS_QUIVER

}// namespace balsa::geometry::simplex

#endif
