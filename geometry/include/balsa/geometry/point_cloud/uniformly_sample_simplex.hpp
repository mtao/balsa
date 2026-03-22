
#if !defined(BALSA_GEOMETRY_POINT_CLOUD_UNIFORMLY_SAMPLE_SIMPLEX_HPP)
#define BALSA_GEOMETRY_POINT_CLOUD_UNIFORMLY_SAMPLE_SIMPLEX_HPP

// Uniform random sampling from an N-simplex.
//
// Delegates to quiver::simplex for the canonical implementation.
// Eigen overloads convert to zipper and forward.

#include <quiver/simplex/uniformly_sample_simplex.hpp>

#include "balsa/eigen/concepts/matrix_types.hpp"
#include "balsa/eigen/zipper_compat.hpp"

#include <cstddef>
#include <random>

namespace balsa::geometry::point_cloud {

// ── Zipper overloads (delegate to quiver) ──────────────────────────────

/// Uniformly sample a single point from an N-simplex.
/// V: D×(N+1) matrix of column-vector vertices.
template<::zipper::concepts::Matrix MatType, std::uniform_random_bit_generator RNG>
auto uniformly_sample_simplex(const MatType &V, RNG &gen) {
    return ::quiver::simplex::uniformly_sample_simplex(gen, V);
}

/// Sample multiple uniform random points from an N-simplex.
/// Returns a D×num_samples matrix.
template<::zipper::concepts::Matrix MatType, std::uniform_random_bit_generator RNG>
auto uniformly_sample_simplex(const MatType &V, std::size_t num_samples, RNG &gen) {
    return ::quiver::simplex::uniformly_sample_simplex(gen, V, num_samples);
}

// ── Eigen overloads (convert to zipper, then delegate) ─────────────────

/// Uniformly sample a single point from an N-simplex (Eigen).
/// V: (dim × (N+1)) matrix of column-vector vertices.
template<eigen::concepts::MatrixBaseDerived Derived, std::uniform_random_bit_generator RNG>
auto uniformly_sample_simplex(const Derived &V, RNG &gen)
  -> Eigen::Vector<typename Derived::Scalar, Derived::RowsAtCompileTime> {
    auto result = ::quiver::simplex::uniformly_sample_simplex(gen, eigen::as_zipper(V));
    return eigen::as_eigen(result);
}

/// Sample multiple uniform random points from an N-simplex (Eigen).
/// Returns a (dim × num_samples) matrix.
template<eigen::concepts::MatrixBaseDerived Derived, std::uniform_random_bit_generator RNG>
auto uniformly_sample_simplex(const Derived &V, int num_samples, RNG &gen)
  -> Eigen::Matrix<typename Derived::Scalar, Derived::RowsAtCompileTime, Eigen::Dynamic> {
    auto result = ::quiver::simplex::uniformly_sample_simplex(
      gen, eigen::as_zipper(V), static_cast<std::size_t>(num_samples));
    return eigen::as_eigen(result);
}

}// namespace balsa::geometry::point_cloud

#endif
