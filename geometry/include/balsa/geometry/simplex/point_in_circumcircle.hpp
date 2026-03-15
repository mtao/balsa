#if !defined(BALSA_GEOMETRY_SIMPLEX_POINT_IN_CIRCUMCIRCLE_HPP)
#define BALSA_GEOMETRY_SIMPLEX_POINT_IN_CIRCUMCIRCLE_HPP

// Point-in-circumcircle test.
//
// Delegates to quiver::simplex::point_in_circumcircle.

#include <quiver/simplex/point_in_circumcircle.hpp>

namespace balsa::geometry::simplex {

template<::zipper::concepts::Matrix SimplexVertices, ::zipper::concepts::Vector PointType>
bool point_in_circumcircle(const SimplexVertices &S, const PointType &P) {
    return ::quiver::simplex::point_in_circumcircle(S, P);
}

}// namespace balsa::geometry::simplex

#endif
