#if !defined(BALSA_GEOMETRY_SIMPLEX_VOLUME_HPP)
#define BALSA_GEOMETRY_SIMPLEX_VOLUME_HPP
#include <numeric>
#include <stdexcept>
#include <fmt/format.h>

#include "balsa/eigen/types.hpp"
#include "balsa/eigen/concepts/matrix_types.hpp"
#include "balsa/eigen/concepts/shape_types.hpp"
#include "balsa/utils/factorial.hpp"

namespace balsa::geometry::simplex {

// signed volume of a simplex, only works for N,N+1 shaped matrices
template<eigen::concepts::MatrixBaseDerived MatType>
auto volume_signed(const MatType &V) -> typename MatType::Scalar;
// unsigned volume of a simplex, only works for N,
template<eigen::concepts::MatrixBaseDerived MatType>
auto volume_unsigned(const MatType &V) -> typename MatType::Scalar;


template<eigen::concepts::MatrixBaseDerived MatType>
auto volume(const MatType &V) -> typename MatType::Scalar;

namespace detail {
    template<eigen::concepts::MatrixBaseDerived MatType>
    auto volume_signed(const MatType &V) {

        auto m = (V.rightCols(V.cols() - 1).colwise() - V.col(0)).eval();

        if constexpr (eigen::concepts::ColStaticCompatible<MatType>) {
            constexpr static int cols = eigen::concepts::detail::compile_col_size<MatType>;
            return m.determinant() / balsa::utils::factorial(cols);
        } else {
            return m.determinant() / balsa::utils::factorial(m.cols());
        }
    }

    template<eigen::concepts::MatrixBaseDerived MatType>
    auto volume_unsigned(const MatType &V) -> typename MatType::Scalar {
        auto m = (V.rightCols(V.cols() - 1).colwise() - V.col(0)).eval();
        if constexpr (eigen::concepts::ColStaticCompatible<MatType>) {
            constexpr static int cols = eigen::concepts::detail::compile_col_size<MatType>;
            constexpr static int quotient = balsa::utils::factorial(cols);
            return std::sqrt((m.transpose() * m).determinant()) / quotient;
        } else {
            int quotient = balsa::utils::factorial(m.cols());
            return std::sqrt((m.transpose() * m).determinant()) / quotient;
        }
    }
}// namespace detail

template<eigen::concepts::MatrixBaseDerived MatType>
auto volume_signed(const MatType &V) -> typename MatType::Scalar {
    constexpr static int rows = eigen::concepts::detail::compile_col_size<MatType>;
    constexpr static int cols = eigen::concepts::detail::compile_col_size<MatType>;
    if constexpr (eigen::concepts::RowColStaticCompatible<MatType>) {
        static_assert(rows + 1 == cols, "volume_signed expected a N,N+1 matrix");
    } else if (V.cols() != V.rows() + 1) {

        throw std::invalid_argument(fmt::format("volume_signed expected a N,N+1 matrix, got got {0},{1} expected {0},{2}", V.rows(), V.cols(), V.rows() + 1));
    }

    return detail::volume_signed(V);
}
template<eigen::concepts::MatrixBaseDerived MatType>
auto volume_unsigned(const MatType &V) -> typename MatType::Scalar {
    return detail::volume_unsigned(V);
}


template<eigen::concepts::MatrixBaseDerived MatType>
auto volume(const MatType &V) -> typename MatType::Scalar {
    if constexpr (eigen::concepts::RowColStaticCompatible<MatType>) {
        constexpr static int rows = eigen::concepts::detail::compile_col_size<MatType>;
        constexpr static int cols = eigen::concepts::detail::compile_col_size<MatType>;
        if constexpr (rows + 1 == cols) {
            return detail::volume_signed(V);
        } else {
            return detail::volume_unsigned(V);
        }
    } else {
        if (V.rows() + 1 == V.cols()) {
            return detail::volume_signed(V);
        } else {
            return detail::volume_unsigned(V);
        }
    }
}

}// namespace balsa::geometry::simplex
#endif// VOLUME_H
