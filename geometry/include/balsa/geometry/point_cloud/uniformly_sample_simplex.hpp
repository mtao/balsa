
#if !defined(BALSA_GEOMETRY_POINT_CLOUD_UNIFORMLY_SAMPLE_SIMPLEX_HPP)
#define BALSA_GEOMETRY_POINT_CLOUD_UNIFORMLY_SAMPLE_SIMPLEX_HPP

#include "balsa/eigen/types.hpp"
#include <algorithm>
#include <array>
#include <random>

namespace balsa::geometry::point_cloud {

// Uniformly sample a random point from an N-simplex defined by its vertices.
//
// Uses the Kraemer-Faulkner method: generate (N) uniform random numbers in [0,1],
// sort them, compute differences to get barycentric coordinates, then combine
// with the simplex vertices.
//
// V: (dim x (N+1)) matrix of column-vector vertices defining the simplex
// gen: random number generator (e.g. std::mt19937)
//
// Returns a (dim x 1) column vector — the sampled point.
template<typename Derived, typename RNG>
auto uniformly_sample_simplex(const Eigen::MatrixBase<Derived> &V, RNG &gen)
  -> Eigen::Vector<typename Derived::Scalar, Derived::RowsAtCompileTime> {
    using Scalar = typename Derived::Scalar;
    constexpr int Rows = Derived::RowsAtCompileTime;

    const int dim = V.rows();
    const int num_vertices = V.cols();// N+1 for an N-simplex
    const int N = num_vertices - 1;

    std::uniform_real_distribution<Scalar> dist(Scalar(0), Scalar(1));

    // Generate N random numbers, sort them
    Eigen::VectorX<Scalar> sorted(N);
    for (int i = 0; i < N; ++i) {
        sorted(i) = dist(gen);
    }
    std::sort(sorted.data(), sorted.data() + N);

    // Barycentric coordinates from differences:
    // lambda_0 = sorted[0], lambda_i = sorted[i] - sorted[i-1], lambda_N = 1 - sorted[N-1]
    Eigen::Vector<Scalar, Rows> result = V.col(0) * sorted(0);
    for (int i = 1; i < N; ++i) {
        result += V.col(i) * (sorted(i) - sorted(i - 1));
    }
    result += V.col(N) * (Scalar(1) - sorted(N - 1));

    return result;
}

// Sample multiple uniform random points from an N-simplex.
//
// V: (dim x (N+1)) matrix of column-vector vertices
// num_samples: number of points to generate
// gen: random number generator
//
// Returns a (dim x num_samples) matrix of column-vector sample points.
template<typename Derived, typename RNG>
auto uniformly_sample_simplex(const Eigen::MatrixBase<Derived> &V, int num_samples, RNG &gen)
  -> Eigen::Matrix<typename Derived::Scalar, Derived::RowsAtCompileTime, Eigen::Dynamic> {
    using Scalar = typename Derived::Scalar;
    constexpr int Rows = Derived::RowsAtCompileTime;

    Eigen::Matrix<Scalar, Rows, Eigen::Dynamic> result(V.rows(), num_samples);
    for (int i = 0; i < num_samples; ++i) {
        result.col(i) = uniformly_sample_simplex(V, gen);
    }
    return result;
}

}// namespace balsa::geometry::point_cloud

#endif
