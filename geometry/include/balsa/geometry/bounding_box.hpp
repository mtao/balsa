#if !defined(BALSA_GEOMETRY_BOUNDING_BOX)
#define BALSA_GEOMETRY_BOUNDING_BOX
#include "BoundingBox.hpp"
#include "balsa/eigen/concepts/shape_types.hpp"
#include "balsa/eigen/types.hpp"
#include <Eigen/Geometry>
#include <zipper/Matrix.hpp>

namespace balsa::geometry {

template <eigen::concepts::ColVecsDCompatible VType>
auto bounding_box(const VType &V) {
    using BBox =
        Eigen::AlignedBox<typename VType::Scalar, VType::RowsAtCompileTime>;
    if (V.cols() > 0) {
        return BBox{V.rowwise().minCoeff(), V.rowwise().maxCoeff()};
    }
    return BBox{};
}

template <::zipper::concepts::Matrix VType>
auto bounding_box(const VType &V) {
    using T = typename VType::value_type;
    constexpr auto Dim = VType::extents_type::static_extent(0);
    BoundingBox<T, Dim> bb;
    for (zipper::index_type j = 0; j < V.extent(1); ++j) {
        bb.expand(V.col(j));
    }
    return bb;
}

} // namespace balsa::geometry

#endif
