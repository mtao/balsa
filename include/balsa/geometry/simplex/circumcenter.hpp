#if !defined(BALSA_GEOMETRY_SIMPLEX_CIRCUMCENTER_HPP)
#define BALSA_GEOMETRY_SIMPLEX_CIRCUMCENTER_HPP
#include <stdexcept>
#include <fmt/format.h>
#include <spdlog/spdlog.h>
#include <balsa/eigen/types.hpp>
#include <tuple>
#include <balsa/eigen/concepts/shape_types.hpp>


#include <Eigen/Dense>

namespace balsa::geometry::simplex {

namespace detail {
    template<eigen::concepts::MatrixBaseDerived MatType>
    auto circumcenter_spsd(const MatType &V) {
        constexpr static int compile_cols = eigen::concepts::detail::compile_col_size<MatType>;
        constexpr static int A_mat_size = eigen::concepts::detail::relative_compile_size(compile_cols, +1);

        eigen::SquareMatrix<typename MatType::Scalar, A_mat_size> A(V.cols() + 1, V.cols() + 1);
        A.setConstant(1);
        A(V.cols(), V.cols()) = 0;
        auto m = V.transpose() * V;

        A.topLeftCorner(V.cols(), V.cols()) = 2 * m;
        eigen::VectorX<typename MatType::Scalar> b(V.cols() + 1);
        b.setConstant(1);
        b.topRows(m.cols()) = V.colwise().squaredNorm().transpose();

        auto x = A.colPivHouseholderQr().solve(b).eval();

        return (V * x.topRows(V.cols())).eval();
    }

    // should be a N,N+1 shape matrix
    // if RowCol static then get a shape guarantee
    template<eigen::concepts::MatrixBaseDerived MatType>
        requires (!eigen::concepts::RowColStaticCompatible<MatType> || eigen::concepts::detail::has_n_more_rows_than_cols<MatType>(-1))
    auto circumcenter_spd(const MatType &V) {
        // 2 V.dot(C) = sum(V.colwise().squaredNorm()).transpose()

        // probably dont really need this temporary
        auto m = (V.rightCols(V.cols() - 1).colwise() - V.col(0)).eval();
        auto A = (2 * m.transpose() * m).eval();
        auto b = m.colwise().squaredNorm().transpose().eval();
        auto llt = A.llt();
        if(llt.info() != Eigen::ComputationInfo::Success)
        {
            spdlog::debug("circumcenter_spd: Degenerate simplex detected, using circumcenter_spsd");
            return circumcenter_spsd(V);
        }

        llt.solveInPlace(b);

        auto c = (V.col(0) + m * b).eval();
        return c;
    }
}// namespace detail


template<eigen::concepts::MatrixBaseDerived MatType>
auto circumcenter_spd(const MatType &V) {
    constexpr static int rows = eigen::concepts::detail::compile_row_size<MatType>;
    constexpr static int cols = eigen::concepts::detail::compile_col_size<MatType>;
    if constexpr (eigen::concepts::RowColStaticCompatible<MatType>) {
        static_assert(rows + 1 == cols, "circumcenter_spd expected a N,N+1 matrix");
    } else if (V.cols() != V.rows() + 1) {

        throw std::invalid_argument(fmt::format("circumcenter_spd expected a N,N+1 matrix, got got {0},{1} expected {0},{2}", V.rows(), V.cols(), V.rows() + 1));
    }

    return detail::circumcenter_spd(V);
}
template<eigen::concepts::MatrixBaseDerived MatType>
auto circumcenter_spsd(const MatType &V) {
    return detail::circumcenter_spsd(V);
}


template<eigen::concepts::MatrixBaseDerived MatType>
auto circumcenter(const MatType &V) {

    if constexpr (eigen::concepts::RowColStaticCompatible<MatType>) {
        constexpr static int rows = eigen::concepts::detail::compile_row_size<MatType>;
        constexpr static int cols = eigen::concepts::detail::compile_col_size<MatType>;
        if constexpr (rows + 1 == cols) {
            return detail::circumcenter_spd(V);
        } else {
            return detail::circumcenter_spsd(V);
        }
    } else {
        if (V.rows() + 1 == V.cols()) {
            return detail::circumcenter_spd(V);
        } else {
            return detail::circumcenter_spsd(V);
        }
    }
}

template<eigen::concepts::MatrixBaseDerived SimplexVertices>
auto circumcenter_with_squared_radius(const SimplexVertices &S) {
    auto C = circumcenter(S);
    return std::make_tuple(C, (C - S.col(0)).squaredNorm());
}

template<eigen::concepts::MatrixBaseDerived SimplexVertices>
auto circumcenter_with_radius(const SimplexVertices &S) {
    auto [C, r2] = circumcenter_with_squared_radius(S);
    return std::make_tuple(C, std::sqrt(r2));
}
}// namespace balsa::geometry::simplex
#endif// CIRCUMCENTER_H
