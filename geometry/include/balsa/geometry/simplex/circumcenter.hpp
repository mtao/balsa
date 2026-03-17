#if !defined(BALSA_GEOMETRY_SIMPLEX_CIRCUMCENTER_HPP)
#define BALSA_GEOMETRY_SIMPLEX_CIRCUMCENTER_HPP

// Circumcenter computation for simplices.
//
// When quiver is available, delegates to quiver::simplex.  Otherwise a
// standalone Eigen-based implementation is provided.

#if defined(BALSA_HAS_QUIVER)
#include <quiver/simplex/circumcenter.hpp>
#endif

#include "balsa/eigen/types.hpp"
#include "balsa/eigen/concepts/matrix_types.hpp"
#include "balsa/eigen/zipper_compat.hpp"
#include <zipper/Vector.hpp>

#include <Eigen/Dense>
#include <cmath>
#include <tuple>
#include <utility>

namespace balsa::geometry::simplex {

#if defined(BALSA_HAS_QUIVER)

// ── Zipper overloads (delegate to quiver) ──────────────────────────────

template<::zipper::concepts::Matrix MatType>
auto circumcenter(const MatType &V) {
    return ::quiver::simplex::circumcenter(V);
}

template<::zipper::concepts::Matrix SimplexVertices>
auto circumcenter_with_squared_radius(const SimplexVertices &S) {
    return ::quiver::simplex::circumcenter_with_squared_radius(S);
}

template<::zipper::concepts::Matrix SimplexVertices>
auto circumcenter_with_radius(const SimplexVertices &S) {
    return ::quiver::simplex::circumcenter_with_radius(S);
}

#else// !BALSA_HAS_QUIVER -- standalone Eigen-based implementation

namespace detail {

    /// Build D x K edge matrix: col i = vertex_{i+1} - vertex_0.
    template<typename Derived>
    auto cc_edge_matrix(const Eigen::MatrixBase<Derived> &V) {
        using Scalar = typename Derived::Scalar;
        const auto D = V.rows();
        const auto K = V.cols() - 1;
        Eigen::Matrix<Scalar, Eigen::Dynamic, Eigen::Dynamic> E(D, K);
        for (Eigen::Index i = 0; i < K; ++i) {
            E.col(i) = V.col(i + 1) - V.col(0);
        }
        return E;
    }

    /// Core circumcenter solver from edge matrix.
    ///   Gram matrix  G = E^T E   (K x K, SPD for non-degenerate simplices)
    ///   RHS          rhs_i = G(i,i) / 2
    ///   Solve        G y = rhs
    ///   Circumcenter = v0 + E * y
    ///   Fallback to centroid on degenerate input.
    template<typename Derived, typename Vec>
    auto cc_from_edges(const Eigen::MatrixBase<Derived> &E,
                       const Eigen::MatrixBase<Vec> &v0) {
        using Scalar = typename Derived::Scalar;
        const auto K = E.cols();

        auto G = (E.transpose() * E).eval();

        Eigen::Matrix<Scalar, Eigen::Dynamic, 1> rhs(K);
        for (Eigen::Index i = 0; i < K; ++i) {
            rhs(i) = G(i, i) / Scalar(2);
        }

        auto ldlt = G.ldlt();
        if (ldlt.info() != Eigen::Success || !ldlt.isPositive()) {
            return (v0 + E.rowwise().mean()).eval();
        }

        Eigen::Matrix<Scalar, Eigen::Dynamic, 1> y = ldlt.solve(rhs);
        return (v0 + E * y).eval();
    }

    /// Copy Eigen vector into an owning zipper::Vector.
    template<typename Scalar, ::zipper::index_type D, typename Derived>
    auto to_zipper_vec(const Eigen::MatrixBase<Derived> &ev, ::zipper::index_type n) {
        ::zipper::Vector<Scalar, D> result(n);
        for (::zipper::index_type i = 0; i < n; ++i) {
            result(i) = ev(i);
        }
        return result;
    }

}// namespace detail

// ── Eigen overloads ────────────────────────────────────────────────────

template<eigen::concepts::MatrixBaseDerived MatType>
auto circumcenter(const MatType &V) {
    auto E = detail::cc_edge_matrix(V);
    return detail::cc_from_edges(E, V.col(0));
}

template<eigen::concepts::MatrixBaseDerived MatType>
auto circumcenter_with_squared_radius(const MatType &V) {
    auto E = detail::cc_edge_matrix(V);
    auto C = detail::cc_from_edges(E, V.col(0));
    auto diff = (C - V.col(0)).eval();
    auto r2 = diff.squaredNorm();
    return std::make_pair(std::move(C), r2);
}

template<eigen::concepts::MatrixBaseDerived MatType>
auto circumcenter_with_radius(const MatType &V) {
    auto [C, r2] = circumcenter_with_squared_radius(V);
    return std::make_pair(std::move(C), std::sqrt(r2));
}

// ── Zipper overloads (convert to Eigen, compute, return zipper) ────────

template<::zipper::concepts::Matrix MatType>
auto circumcenter(const MatType &V) {
    using Scalar = typename MatType::value_type;
    constexpr auto D = MatType::extents_type::static_extent(0);
    auto eigen_result = circumcenter(eigen::as_eigen(V));
    return detail::to_zipper_vec<Scalar, D>(eigen_result, V.extent(0));
}

template<::zipper::concepts::Matrix MatType>
auto circumcenter_with_squared_radius(const MatType &V) {
    using Scalar = typename MatType::value_type;
    constexpr auto D = MatType::extents_type::static_extent(0);
    auto [eigen_C, r2] = circumcenter_with_squared_radius(eigen::as_eigen(V));
    auto C = detail::to_zipper_vec<Scalar, D>(eigen_C, V.extent(0));
    return std::make_pair(std::move(C), r2);
}

template<::zipper::concepts::Matrix MatType>
auto circumcenter_with_radius(const MatType &V) {
    using Scalar = typename MatType::value_type;
    constexpr auto D = MatType::extents_type::static_extent(0);
    auto [eigen_C, r] = circumcenter_with_radius(eigen::as_eigen(V));
    auto C = detail::to_zipper_vec<Scalar, D>(eigen_C, V.extent(0));
    return std::make_pair(std::move(C), r);
}

#endif// BALSA_HAS_QUIVER

}// namespace balsa::geometry::simplex
#endif// BALSA_GEOMETRY_SIMPLEX_CIRCUMCENTER_HPP
