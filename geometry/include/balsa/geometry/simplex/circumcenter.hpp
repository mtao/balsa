#if !defined(BALSA_GEOMETRY_SIMPLEX_CIRCUMCENTER_HPP)
#define BALSA_GEOMETRY_SIMPLEX_CIRCUMCENTER_HPP

// Circumcenter computation for simplices.
//
// The zipper-concept implementations live in quiver::simplex and are
// re-exported here for backward compatibility.

#include <quiver/simplex/circumcenter.hpp>

namespace balsa::geometry::simplex {

// ── Zipper overloads (delegate to quiver) ──────────────────────────────

template<::zipper::concepts::Matrix MatType>
    requires(MatType::extents_traits::is_dynamic || MatType::extents_type::static_extent(0) + 1 == MatType::extents_type::static_extent(1))
auto circumcenter_spd(const MatType &V) {
    return ::quiver::simplex::circumcenter_spd(V);
}

template<::zipper::concepts::Matrix MatType>
auto circumcenter_spsd(const MatType &V) {
    return ::quiver::simplex::circumcenter_spsd(V);
}

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

}// namespace balsa::geometry::simplex
#endif// BALSA_GEOMETRY_SIMPLEX_CIRCUMCENTER_HPP
