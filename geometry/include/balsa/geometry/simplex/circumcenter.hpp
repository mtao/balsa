#if !defined(BALSA_GEOMETRY_SIMPLEX_CIRCUMCENTER_HPP)
#define BALSA_GEOMETRY_SIMPLEX_CIRCUMCENTER_HPP
#include <zipper/utils/format.hpp>
#include <spdlog/spdlog.h>
#include <stdexcept>
#include <zipper/utils/extents/extent_arithmetic.hpp>
#include <zipper/expression/nullary/Constant.hpp>
#include <zipper/utils/decomposition/qr.hpp>
#include <tuple>
#include "balsa/zipper/types.hpp"
#include "zipper/concepts/Matrix.hpp"
#include "zipper/concepts/Vector.hpp"
#include "zipper/concepts/Zipper.hpp"

namespace balsa::geometry::simplex {


template<::zipper::concepts::Matrix MatType>
    requires(MatType::extents_traits::is_dynamic || MatType::extents_type::static_extent(0) + 1 == MatType::extents_type::static_extent(1))
auto circumcenter_spd(const MatType &V);
template<::zipper::concepts::Matrix MatType>
auto circumcenter_spsd(const MatType &V);


template<::zipper::concepts::Matrix MatType>
auto circumcenter(const MatType &V);

template<::zipper::concepts::Matrix SimplexVertices>
auto circumcenter_with_squared_radius(const SimplexVertices &S);

template<::zipper::concepts::Matrix SimplexVertices>
auto circumcenter_with_radius(const SimplexVertices &S);


namespace detail {

    // Back-substitution for upper triangular system R*x = b.
    // Near-zero pivots are treated as zero (for rank-deficient systems).
    template<::zipper::concepts::Matrix RMat, ::zipper::concepts::Zipper BVec>
    auto upper_triangular_solve(const RMat &R, const BVec &b) {
        using value_type = typename RMat::value_type;
        constexpr auto N = RMat::static_extent(1);
        ::zipper::Vector<value_type, N> x(R.extent(1));
        const value_type pivot_tol = std::numeric_limits<value_type>::epsilon() * 100;
        for (::zipper::index_type ii = 0; ii < R.extent(1); ++ii) {
            ::zipper::index_type i = R.extent(1) - 1 - ii;
            value_type s = b(i);
            for (::zipper::index_type j = i + 1; j < R.extent(1); ++j) {
                s -= R(i, j) * x(j);
            }
            x(i) = (std::abs(R(i, i)) > pivot_tol) ? s / R(i, i) : value_type(0);
        }
        return x;
    }

    // Solve A*x = b via QR decomposition with back-substitution.
    template<::zipper::concepts::Matrix AMat, ::zipper::concepts::Zipper BVec>
    auto qr_solve(const AMat &A, const BVec &b) {
        auto [Q, R] = ::zipper::utils::decomposition::qr(A);
        auto Qtb = (Q.transpose() * b).eval();
        return upper_triangular_solve(R, Qtb);
    }

    // Check if the QR R factor indicates a rank-deficient matrix
    // (any diagonal element near zero).
    template<::zipper::concepts::Matrix RMat>
    bool qr_is_degenerate(const RMat &R) {
        using value_type = typename RMat::value_type;
        const value_type tol = std::numeric_limits<value_type>::epsilon() * 100;
        ::zipper::index_type n = std::min(R.extent(0), R.extent(1));
        for (::zipper::index_type i = 0; i < n; ++i) {
            if (std::abs(R(i, i)) < tol) {
                return true;
            }
        }
        return false;
    }

    template<::zipper::concepts::Matrix MatType>
    auto circumcenter_spsd(const MatType &V) {
        using value_type = typename MatType::value_type;
        constexpr static ::zipper::index_type compile_cols = MatType::static_extent(1);
        constexpr static ::zipper::index_type Msize = ::zipper::utils::extents::plus(compile_cols, 1);
        using MType = ::zipper::Matrix<value_type, Msize, Msize>;


        constexpr static bool static_cols = !MatType::extents_traits::is_dynamic_extent(1);
        zipper::index_type simplex_count = V.cols();
        auto A = MType(simplex_count + 1, simplex_count + 1);


        auto ones = ::zipper::expression::nullary::Constant<value_type>(1);
        A.row(simplex_count).head(A.extent(0) - 1) = ones;
        A.col(simplex_count).head(A.extent(1) - 1) = ones;
        A(simplex_count, simplex_count) = 0;
        if constexpr (static_cols) {
            auto m = A.template slice<
              ::zipper::static_slice_t<0, compile_cols, 1>,//
              ::zipper::static_slice_t<0, compile_cols, 1>//
              >();
            m = 2 * V.transpose() * V;


        } else {
            auto m = A.slice(
              ::zipper::slice({}, V.extent(1)),
              ::zipper::slice({}, V.extent(1)));
            m = 2 * V.transpose() * V;
        }
        auto b = V.colwise().norm_powered().homogeneous().eval();

        auto x = qr_solve(A, b);

        return (V * x.head(V.cols())).eval();
    }

    // should be a N,N+1 shape matrix
    // if RowCol static then get a shape guarantee
    template<::zipper::concepts::Matrix MatType>
        requires(MatType::extents_traits::is_dynamic || (MatType::extents_type::static_extent(0) + 1 == MatType::extents_type::static_extent(1)))
    auto circumcenter_spd(const MatType &V) {
        // 2 V.dot(C) = sum(V.colwise().squaredNorm()).transpose()

        assert(V.extent(0) + 1 == V.extent(1));
        // probably dont really need this temporary
        using ET = MatType::extents_type;
        using ETraits = MatType::extents_traits;
        constexpr static ::zipper::index_type static_cols = !ETraits::is_dynamic_extent(0) ? ET::static_extent(0) + 1 : ET::static_extent(1);
        constexpr static bool has_static_cols = static_cols != std::dynamic_extent;

        // todo fix this construction in zipper
        auto m = [](const auto &VV) {
            if constexpr (has_static_cols) {
                using slice_t = ::zipper::static_slice_t<1, static_cols - 1, 1>;
                return VV.template slice<::zipper::full_extent_t, slice_t>();
            } else {
                return VV.template slice<::zipper::full_extent_t>(
                  ::zipper::full_extent_t{},
                  ::zipper::slice(
                    ::zipper::static_index_t<1>{},
                    VV.extent(1) - 1));
            }
        }(V)
                   .eval();


        // todo fix this construction in zipper
        for (::zipper::index_type j = 0; j < m.extent(1); ++j) {
            auto mv = m.col(j);
            mv = mv - V.col(0);
        }
        auto b = m.colwise().norm_powered().eval();
        auto A = (2 * m.transpose() * m).eval();

        auto [Q, R] = ::zipper::utils::decomposition::qr(A);
        if (qr_is_degenerate(R)) {
            spdlog::debug("circumcenter_spd: Degenerate simplex detected, using circumcenter_spsd");
            return circumcenter_spsd(V);
        }

        auto Qtb = (Q.transpose() * b).eval();
        auto sol = upper_triangular_solve(R, Qtb);

        return (V.col(0) + m * sol).eval();
    }

}// namespace detail


template<::zipper::concepts::Matrix MatType>
    requires(MatType::extents_traits::is_dynamic || MatType::extents_type::static_extent(0) + 1 == MatType::extents_type::static_extent(1))
auto circumcenter_spd(const MatType &V) {

    return detail::circumcenter_spd(V);
}
template<::zipper::concepts::Matrix MatType>
auto circumcenter_spsd(const MatType &V) {
    return detail::circumcenter_spsd(V);
}


template<::zipper::concepts::Matrix MatType>
auto circumcenter(const MatType &V) {
    // auto v = eigen::as_eigen(V);
    // auto c = circumcenter(v);
    // return eigen::as_zipper(c).eval();
    using extents_type = typename MatType::extents_type;
    using extents_traits = typename MatType::extents_traits;
    constexpr static ::zipper::index_type static_rows = extents_type::static_extent(0);
    constexpr static ::zipper::index_type static_cols = extents_type::static_extent(1);

    if constexpr (extents_traits::is_static) {
        if constexpr (static_cols == 2) {
            return ((V.col(0) + V.col(1)) / typename MatType::value_type(2)).eval();
        } else if constexpr (static_cols == static_rows + 1) {
            return detail::circumcenter_spd(V);
        } else {
            return detail::circumcenter_spsd(V);
        }
    } else {
        using RetType = ::zipper::Vector<typename MatType::value_type, extents_type::static_extent(0)>;
        if (V.extent(1) == 2) {
            return RetType((V.col(0) + V.col(1)) / typename MatType::value_type(2));
        } else if (V.extent(0) + 1 == V.extent(1)) {
            return RetType(detail::circumcenter_spd(V));
        } else {
            return RetType(detail::circumcenter_spsd(V));
        }
    }
}

template<::zipper::concepts::Matrix SimplexVertices>
auto circumcenter_with_squared_radius(const SimplexVertices &S) {
    auto C = circumcenter(S);
    return std::make_tuple(C, (C - S.col(0)).template norm_powered<2>());
}

template<::zipper::concepts::Matrix SimplexVertices>
auto circumcenter_with_radius(const SimplexVertices &S) {
    auto C = circumcenter(S);
    return std::make_tuple(C, (C - S.col(0)).norm());
}


}// namespace balsa::geometry::simplex
#endif// CIRCUMCENTER_H
