#if !defined(BALSA_GEOMETRY_SIMPLEX_POINT_IN_CIRCUMCIRCLE_HPP)
#define BALSA_GEOMETRY_SIMPLEX_POINT_IN_CIRCUMCIRCLE_HPP

#include "circumcenter.hpp"
namespace balsa::geometry::simplex {


template<eigen::concepts::ColStaticCompatible SimplexVertices, eigen::concepts::VecCompatible PointType>
bool point_in_circumcircle(const SimplexVertices &S, const PointType &P) {

    auto [C, r] = circumcenter_with_squared_radius(S);
    return (C - P).squaredNorm() < r;
}

}// namespace balsa::geometry::simplex

#endif
