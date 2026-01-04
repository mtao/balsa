#if !defined(BALSA_EIGEN_SPAN_HPP)
#define BALSA_EIGEN_SPAN_HPP

#include <Eigen/Dense>
#include <span>
#include "balsa/eigen/concepts/shape_types.hpp"


namespace balsa::eigen {
// Returns a span type
template<concepts::PlainObjectBaseDerived Matrix>
auto eigen2span(Matrix &&A) {
    using Mat = std::decay_t<Matrix>;
    constexpr static int Rows = Mat::RowsAtCompileTime;
    constexpr static int Cols = Mat::ColsAtCompileTime;

    using Scalar = typename Mat::Scalar;
    using ConstCorrectScalar = std::conditional_t<std::is_const_v<typename std::remove_reference_t<Matrix>>, const Scalar, Scalar>;
    if constexpr (Rows == Eigen::Dynamic || Cols == Eigen::Dynamic) {
        return std::span<ConstCorrectScalar>(A.data(), A.size());
    } else {
        constexpr static int Size = Rows * Cols;
        return std::span<ConstCorrectScalar, Size>(A.data(), Size);
    }
}
}// namespace balsa::eigen
#endif
