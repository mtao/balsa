#if !defined(BALSA_GEOMETRY_SIMPLEX_POINT_IN_CIRCUMCIRCLE_HPP)
#define BALSA_GEOMETRY_SIMPLEX_POINT_IN_CIRCUMCIRCLE_HPP

// Point-in-circumsphere test.
//
// Delegates to quiver::simplex::point_in_circumsphere.
//
// Note: quiver renamed this from "circumcircle" to "circumsphere" and
// changed the argument order to (P, simplex_vertices).  We provide both
// the new name and a deprecated backward-compatible wrapper.

#include <quiver/simplex/point_in_circumcircle.hpp>

namespace balsa::geometry::simplex {

/// Test whether point P lies inside the circumsphere of the simplex
/// whose vertices are the columns of S.
template<::zipper::concepts::Vector PointType, ::zipper::concepts::Matrix SimplexVertices>
bool point_in_circumsphere(const PointType &P, const SimplexVertices &S) {
    return ::quiver::simplex::point_in_circumsphere(P, S);
}

/// @deprecated Use point_in_circumsphere(P, S) instead.
/// Note: argument order was (S, P) in the old API — this wrapper
/// preserves the old call-site order for backward compatibility.
template<::zipper::concepts::Matrix SimplexVertices, ::zipper::concepts::Vector PointType>
[[deprecated("use point_in_circumsphere(P, S) instead — note swapped argument order")]]
bool point_in_circumcircle(const SimplexVertices &S, const PointType &P) {
    return ::quiver::simplex::point_in_circumsphere(P, S);
}

}// namespace balsa::geometry::simplex

#endif
