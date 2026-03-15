#if !defined(BALSA_GEOMETRY_SIMPLEX_VOLUME_HPP)
#define BALSA_GEOMETRY_SIMPLEX_VOLUME_HPP
#include <spdlog/spdlog.h>
#include <numeric>
#include <stdexcept>
#include <fmt/format.h>
#include "balsa/zipper/types.hpp"
#include <zipper/concepts/Matrix.hpp>
#include <zipper/utils/determinant.hpp>

#include "balsa/eigen/types.hpp"
#include "balsa/eigen/concepts/matrix_types.hpp"
#include "balsa/eigen/concepts/shape_types.hpp"
#include "balsa/utils/factorial.hpp"
#include "balsa/eigen/zipper_compat.hpp"

namespace balsa::geometry::simplex {

// signed volume of a simplex, only works for N,N+1 shaped matrices
template<eigen::concepts::MatrixBaseDerived MatType>
auto volume_signed(const MatType &V) -> typename MatType::Scalar;
// unsigned volume of a simplex, only works for N,
template<eigen::concepts::MatrixBaseDerived MatType>
auto volume_unsigned(const MatType &V) -> typename MatType::Scalar;

template<eigen::concepts::MatrixBaseDerived MatType>
auto volume(const MatType &V) -> typename MatType::Scalar;

// signed volume of a simplex, only works for N,N+1 shaped matrices
template<::zipper::concepts::Matrix MatType>
    requires(MatType::extents_traits::is_dynamic || MatType::extents_type::static_extent(0) + 1 == MatType::extents_type::static_extent(1))
auto volume_signed(const MatType &V) -> typename MatType::value_type;

template<::zipper::concepts::Matrix MatType>
auto volume_unsigned(const MatType &V) -> typename MatType::value_type;

template<::zipper::concepts::Matrix MatType>
auto volume(const MatType &V) -> typename MatType::value_type;


namespace detail {

    template<::zipper::concepts::Matrix SimplexVertices>
    auto volume_signed(const SimplexVertices &V) -> SimplexVertices::value_type {
        spdlog::trace("Volume signed");

        // auto m = V.rightCols(V.cols() - 1).eval();
        // m -= V.col(0).repeat_right();
        // auto m = (V.rightCols(V.cols() - 1) - V.col(0).repeat_right()).eval();
        auto m = V.rightCols(V.cols() - 1).eval();
        for (zipper::index_type j = 0; j < m.cols(); ++j) {
            auto v = m.col(j);
            v = v - V.col(0);
        }


        if constexpr (
          auto cols = ::zipper::detail::extents::get_extent<1>(m.extents());
          cols.is_compiletime()) {
            return ::zipper::utils::determinant(m) / balsa::utils::factorial(cols.value());

        } else if (
          auto rows = ::zipper::detail::extents::get_extent<0>(m.extents());
          rows.is_compiletime()) {
            return ::zipper::utils::determinant(m) / balsa::utils::factorial(rows.value());
        } else {
            return ::zipper::utils::determinant(m) / balsa::utils::factorial(m.cols());
        }
    }

    template<::zipper::concepts::Matrix SimplexVertices>
    auto volume_unsigned(const SimplexVertices &V) -> typename SimplexVertices::value_type {
        spdlog::trace("Volume unsigned");
        auto m = V.rightCols(V.cols() - 1).eval();
        for (zipper::index_type j = 0; j < m.cols(); ++j) {
            auto v = m.col(j);
            v = v - V.col(0);
        }
        if constexpr (
          auto cols = ::zipper::detail::extents::get_extent<1>(m.extents());
          cols.is_compiletime()) {
            constexpr static int quotient = balsa::utils::factorial(cols.value());
            return std::sqrt(::zipper::utils::determinant(m.transpose() * m)) / quotient;
        } else {
            int quotient = balsa::utils::factorial(m.cols());
            return std::sqrt(::zipper::utils::determinant(m.transpose() * m)) / quotient;
        }
    }
}// namespace detail

template<eigen::concepts::MatrixBaseDerived MatType>
auto volume_signed(const MatType &V) -> typename MatType::Scalar {
    constexpr static int rows = eigen::concepts::detail::compile_row_size<MatType>;
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
    constexpr static int rows = eigen::concepts::detail::compile_row_size<MatType>;
    constexpr static int cols = eigen::concepts::detail::compile_col_size<MatType>;
    if constexpr (eigen::concepts::RowColStaticCompatible<MatType>) {
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


// signed volume of a simplex, only works for N,N+1 shaped matrices
template<::zipper::concepts::Matrix MatType>
    requires(MatType::extents_traits::is_dynamic || MatType::extents_type::static_extent(0) + 1 == MatType::extents_type::static_extent(1))
auto volume_signed(const MatType &V) -> typename MatType::value_type {
    auto cols = ::zipper::detail::extents::get_extent<1>(V.extents());
    auto rows = ::zipper::detail::extents::get_extent<0>(V.extents());
    if constexpr (rows.is_compiletime() && cols.is_compiletime()) {
        static_assert(rows.value() + 1 == cols.value(), "volume_signed expected a N,N+1 matrix");
    } else if (V.cols() != V.rows() + 1) {
        throw std::invalid_argument(fmt::format("volume_signed expected a N,N+1 matrix, got got {0},{1} expected {0},{2}", V.rows(), V.cols(), V.rows() + 1));
    }

    return detail::volume_signed(V);
}

template<::zipper::concepts::Matrix MatType>
auto volume_unsigned(const MatType &V) -> typename MatType::value_type {
    return detail::volume_unsigned(V);
}


template<::zipper::concepts::Matrix MatType>
auto volume(const MatType &V) -> typename MatType::value_type {
    auto cols = ::zipper::detail::extents::get_extent<1>(V.extents());
    auto rows = ::zipper::detail::extents::get_extent<0>(V.extents());
    if constexpr (rows.is_compiletime() && cols.is_compiletime()) {
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
