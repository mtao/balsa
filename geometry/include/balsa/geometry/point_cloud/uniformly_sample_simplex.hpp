
#if !defined(BALSA_GEOMETRY_POINT_CLOUD_UNIFORMLY_SAMPLE_SIMPLEX_HPP)
#define BALSA_GEOMETRY_POINT_CLOUD_UNIFORMLY_SAMPLE_SIMPLEX_HPP

// Uniform random sampling from an N-simplex.
//
// Wraps balsa::geometry::simplex::uniformly_sample_simplex with a
// (vertices, rng) argument order.

#include "balsa/geometry/simplex/uniformly_sample_simplex.hpp"

#include <cstddef>
#include <random>

namespace balsa::geometry::point_cloud {

// ── Zipper overloads  ──────────────────────────────

/// Uniformly sample a single point from an N-simplex.
/// V: D×(N+1) matrix of column-vector vertices.
template<::zipper::concepts::Matrix MatType, std::uniform_random_bit_generator RNG>
auto uniformly_sample_simplex(const MatType &V, RNG &gen) {
    return simplex::uniformly_sample_simplex(gen, V);
}

/// Sample multiple uniform random points from an N-simplex.
/// Returns a D×num_samples matrix.
template<::zipper::concepts::Matrix MatType, std::uniform_random_bit_generator RNG>
auto uniformly_sample_simplex(const MatType &V, std::size_t num_samples, RNG &gen) {
    return simplex::uniformly_sample_simplex(gen, V, num_samples);
}

}// namespace balsa::geometry::point_cloud

#endif
