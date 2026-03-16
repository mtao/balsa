#if !defined(BALSA_GEOMETRY_SIMPLEX_CIRCUMCENTER_HPP)
#define BALSA_GEOMETRY_SIMPLEX_CIRCUMCENTER_HPP

// Circumcenter computation for simplices.
//
// The zipper-concept implementations live in quiver::simplex and are
// re-exported here for backward compatibility.
//
// Note: the old circumcenter_spd() and circumcenter_spsd() functions
// have been removed.  Quiver now provides a single unified
// circumcenter() that handles both SPD and SPSD cases internally.

#include <quiver/simplex/circumcenter.hpp>

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

}// namespace balsa::geometry::simplex
#endif// BALSA_GEOMETRY_SIMPLEX_CIRCUMCENTER_HPP
