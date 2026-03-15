#if !defined(BALSA_GEOMETRY_SIMPLEX_POINT_IN_CIRCUMCIRCLE_HPP)
#define BALSA_GEOMETRY_SIMPLEX_POINT_IN_CIRCUMCIRCLE_HPP

#include <zipper/concepts/Matrix.hpp>
#include <zipper/concepts/Vector.hpp>
#include "circumcenter.hpp"
namespace balsa::geometry::simplex {


template<::zipper::concepts::Matrix SimplexVertices, ::zipper::concepts::Vector PointType>
bool point_in_circumcircle(const SimplexVertices &S, const PointType &P) {

    auto [C, r2] = circumcenter_with_squared_radius(S);
    return (C - P).template norm_powered<2>() < r2;
}

}// namespace balsa::geometry::simplex

#endif
