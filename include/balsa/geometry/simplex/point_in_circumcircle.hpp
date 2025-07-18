#if !defined(BALSA_GEOMETRY_SIMPLEX_POINT_IN_CIRCUMCIRCLE_HPP)
#define BALSA_GEOMETRY_SIMPLEX_POINT_IN_CIRCUMCIRCLE_HPP

#include <zipper/concepts/MatrixBaseDerived.hpp>
#include <zipper/concepts/ZipperBaseDerived.hpp>
#include "circumcenter.hpp"
namespace balsa::geometry::simplex {


template<eigen::concepts::ColStaticCompatible SimplexVertices, eigen::concepts::VecCompatible PointType>
bool point_in_circumcircle(const SimplexVertices &S, const PointType &P) {

    auto [C, r2] = circumcenter_with_squared_radius(S);
    return (C - P).squaredNorm() < r2;
}
template<zipper::concepts::MatrixBaseDerived SimplexVertices,zipper::concepts::VectorBaseDerived PointType>
bool point_in_circumcircle(const SimplexVertices &S, const PointType &P) {

    auto [C, r2] = circumcenter_with_squared_radius(S);
    return (C - P).squaredNorm() < r2;
}

}// namespace balsa::geometry::simplex

#endif
