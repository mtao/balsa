#if !defined(BALSA_GEOMETRY_BOUNDING_BOX)
#define BALSA_GEOMETRY_BOUNDING_BOX
#include <Eigen/Geometry>
#include "balsa/eigen/types.hpp"
#include "balsa/eigen/concepts/shape_types.hpp"


namespace balsa::geometry {
template<eigen::concepts::ColVecsDCompatible VType>
auto bounding_box(const VType &V) {
    using BBox = Eigen::AlignedBox<typename VType::Scalar, VType::RowsAtCompileTime>;
    if (V.cols() > 0) {
        return BBox{ V.rowwise().minCoeff(), V.rowwise().maxCoeff() };
    }
    return BBox{};
}
}// namespace balsa::geometry

#endif
