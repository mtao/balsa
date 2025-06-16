#if !defined(BALSA_GEOMETRY_BOUNDING_BOX)
#define BALSA_GEOMETRY_BOUNDING_BOX
#include <Eigen/Geometry>
#include "balsa/eigen/types.hpp"
#include "balsa/eigen/concepts/shape_types.hpp"
#include <zipper/Matrix.hpp>
#include "BoundingBox.hpp"


namespace balsa::geometry {


template<eigen::concepts::ColVecsDCompatible VType>
auto bounding_box(const VType &V) {
    using BBox = Eigen::AlignedBox<typename VType::Scalar, VType::RowsAtCompileTime>;
    if (V.cols() > 0) {
        return BBox{ V.rowwise().minCoeff(), V.rowwise().maxCoeff() };
    }
    return BBox{};
}
template<zipper::concepts::MatrixBaseDerived VType>
auto bounding_box(const VType &V) {
    BoundingBox<typename VType::value_type, VType::extents_type::static_extent(0)> bb;
    for (zipper::index_type j = 0; j < V.extent(1); ++j) {
        bb.expand(V.col(j));
    }
    return bb;
}
}// namespace balsa::geometry

#endif
