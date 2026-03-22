#if !defined(BALSA_GEOMETRY_SIMPLEX_CIRCUMCENTER_HPP)
#define BALSA_GEOMETRY_SIMPLEX_CIRCUMCENTER_HPP

// Circumcenter computation for simplices.
//
// Delegates to quiver::simplex for the canonical implementations.
// Eigen overloads convert to zipper and forward.

#include <quiver/simplex/circumcenter.hpp>

#include "balsa/eigen/concepts/matrix_types.hpp"
#include "balsa/eigen/zipper_compat.hpp"

namespace balsa::geometry::simplex {

// ── Zipper overloads (delegate to quiver) ──────────────────────────────

template<::zipper::concepts::Matrix MatType>
auto circumcenter(const MatType &V) {
    return ::quiver::simplex::circumcenter(V);
}

template<::zipper::concepts::Matrix SimplexVertices>
auto circumcenter_with_squared_radius(const SimplexVertices &S) {
    return ::quiver::simplex::circumcenter_with_squared_radius(S);
}

template<::zipper::concepts::Matrix SimplexVertices>
auto circumcenter_with_radius(const SimplexVertices &S) {
    return ::quiver::simplex::circumcenter_with_radius(S);
}

// ── Eigen overloads (convert to zipper, then delegate) ─────────────────

template<eigen::concepts::MatrixBaseDerived MatType>
auto circumcenter(const MatType &V) {
    return ::quiver::simplex::circumcenter(eigen::as_zipper(V));
}

template<eigen::concepts::MatrixBaseDerived MatType>
auto circumcenter_with_squared_radius(const MatType &V) {
    return ::quiver::simplex::circumcenter_with_squared_radius(eigen::as_zipper(V));
}

template<eigen::concepts::MatrixBaseDerived MatType>
auto circumcenter_with_radius(const MatType &V) {
    return ::quiver::simplex::circumcenter_with_radius(eigen::as_zipper(V));
}

}// namespace balsa::geometry::simplex
#endif// BALSA_GEOMETRY_SIMPLEX_CIRCUMCENTER_HPP
