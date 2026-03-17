#if !defined(BALSA_GEOMETRY_SIMPLEX_VOLUME_HPP)
#define BALSA_GEOMETRY_SIMPLEX_VOLUME_HPP

// Simplex volume computation.
//
// When quiver is available the zipper-concept implementations delegate to
// quiver::simplex and are re-exported here for backward compatibility.
// Otherwise a standalone Eigen-based implementation is provided.
// The Eigen overloads convert to zipper (or compute directly) and delegate.

#if defined(BALSA_HAS_QUIVER)
#include <quiver/simplex/volume.hpp>
#endif

#include "balsa/eigen/types.hpp"
#include "balsa/eigen/concepts/matrix_types.hpp"
#include "balsa/eigen/concepts/shape_types.hpp"
#include "balsa/eigen/zipper_compat.hpp"

#include <cmath>

namespace balsa::geometry::simplex {

#if defined(BALSA_HAS_QUIVER)

// ── Zipper overloads (delegate to quiver) ──────────────────────────────

template<::zipper::concepts::Matrix MatType>
auto volume_signed(const MatType &V) -> typename MatType::value_type {
    return ::quiver::simplex::volume_signed(V);
}

template<::zipper::concepts::Matrix MatType>
auto volume_unsigned(const MatType &V) -> typename MatType::value_type {
    return ::quiver::simplex::volume_unsigned(V);
}

/// Convenience wrapper: signed when D+1 == cols (full-rank simplex),
/// unsigned otherwise.  Quiver has no direct equivalent.
template<::zipper::concepts::Matrix MatType>
auto volume(const MatType &V) -> typename MatType::value_type {
    if (V.extent(0) + 1 == V.extent(1)) {
        return ::quiver::simplex::volume_signed(V);
    } else {
        return ::quiver::simplex::volume_unsigned(V);
    }
}

// ── Eigen overloads (convert to zipper, then delegate) ─────────────────

template<eigen::concepts::MatrixBaseDerived MatType>
auto volume_signed(const MatType &V) -> typename MatType::Scalar {
    return ::quiver::simplex::volume_signed(eigen::as_zipper(V));
}

template<eigen::concepts::MatrixBaseDerived MatType>
auto volume_unsigned(const MatType &V) -> typename MatType::Scalar {
    return ::quiver::simplex::volume_unsigned(eigen::as_zipper(V));
}

template<eigen::concepts::MatrixBaseDerived MatType>
auto volume(const MatType &V) -> typename MatType::Scalar {
    auto Z = eigen::as_zipper(V);
    if (Z.extent(0) + 1 == Z.extent(1)) {
        return ::quiver::simplex::volume_signed(Z);
    } else {
        return ::quiver::simplex::volume_unsigned(Z);
    }
}

#else// !BALSA_HAS_QUIVER -- standalone Eigen-based implementation

namespace detail {

    constexpr int factorial(int n) {
        int r = 1;
        for (int i = 2; i <= n; ++i) r *= i;
        return r;
    }

    /// Build D x K edge matrix: col i = vertex_{i+1} - vertex_0.
    /// V is D x (K+1).
    template<typename Derived>
    auto edge_matrix(const Eigen::MatrixBase<Derived> &V) {
        using Scalar = typename Derived::Scalar;
        const auto D = V.rows();
        const auto K = V.cols() - 1;// number of edges
        Eigen::Matrix<Scalar, Eigen::Dynamic, Eigen::Dynamic> E(D, K);
        for (Eigen::Index i = 0; i < K; ++i) {
            E.col(i) = V.col(i + 1) - V.col(0);
        }
        return E;
    }

}// namespace detail

template<eigen::concepts::MatrixBaseDerived MatType>
auto volume_signed(const MatType &V) -> typename MatType::Scalar {
    auto E = detail::edge_matrix(V);
    return E.determinant() / detail::factorial(static_cast<int>(E.cols()));
}

template<eigen::concepts::MatrixBaseDerived MatType>
auto volume_unsigned(const MatType &V) -> typename MatType::Scalar {
    using std::abs;
    using std::sqrt;
    auto E = detail::edge_matrix(V);
    const int K = static_cast<int>(E.cols());
    if (E.rows() == E.cols()) {
        return abs(E.determinant()) / detail::factorial(K);
    }
    // Embedded case: Gram determinant
    auto G = (E.transpose() * E).eval();
    return sqrt(abs(G.determinant())) / detail::factorial(K);
}

template<eigen::concepts::MatrixBaseDerived MatType>
auto volume(const MatType &V) -> typename MatType::Scalar {
    if (V.rows() + 1 == V.cols()) {
        return volume_signed(V);
    } else {
        return volume_unsigned(V);
    }
}

// ── Zipper overloads (convert to Eigen, then compute) ──────────────────

template<::zipper::concepts::Matrix MatType>
auto volume_signed(const MatType &V) -> typename MatType::value_type {
    return volume_signed(eigen::as_eigen(V));
}

template<::zipper::concepts::Matrix MatType>
auto volume_unsigned(const MatType &V) -> typename MatType::value_type {
    return volume_unsigned(eigen::as_eigen(V));
}

template<::zipper::concepts::Matrix MatType>
auto volume(const MatType &V) -> typename MatType::value_type {
    return volume(eigen::as_eigen(V));
}

#endif// BALSA_HAS_QUIVER

}// namespace balsa::geometry::simplex
#endif// BALSA_GEOMETRY_SIMPLEX_VOLUME_HPP
