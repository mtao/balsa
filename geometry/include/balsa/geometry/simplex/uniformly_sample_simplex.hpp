#if !defined(BALSA_GEOMETRY_SIMPLEX_UNIFORMLY_SAMPLE_SIMPLEX_HPP)
#define BALSA_GEOMETRY_SIMPLEX_UNIFORMLY_SAMPLE_SIMPLEX_HPP

/// @file uniformly_sample_simplex.hpp
/// @brief Uniform random sampling from an N-simplex.
///
/// Four-layer design:
///   - **Layer 1 (zipper-concepts core)**: constrained on
///     `::zipper::concepts::Vector`.
///   - **Layer 1.5 (PointSpan wrappers)**: thin overloads converting
///     `std::span<const T, N>` to zipper via `VectorBase(span)` CTAD.
///   - **Layer 1.5M (Matrix wrappers)**: overloads accepting a D×N
///     matrix of vertex columns.
///   - **Layer 1.5ID (HasAsTuple wrappers)**: for IncidentData and similar.
///
/// Uses the Kraemer-Faulkner method: generate K uniform random numbers
/// in [0,1], sort them, compute barycentric coordinates from
/// differences, then combine with the simplex vertices.

#include <balsa/geometry/simplex/concepts.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <random>
#include <span>
#include <tuple>
#include <utility>

#include <zipper/Matrix.hpp>
#include <zipper/Vector.hpp>
#include <zipper/VectorBase.hpp>
#include <zipper/concepts/Matrix.hpp>
#include <zipper/concepts/Vector.hpp>

namespace balsa::geometry::simplex {

// ============================================================================
// Layer 1: zipper-concepts core
// ============================================================================

/// Uniformly sample a random point from an N-simplex defined by its
/// (N+1) vertex vectors (zipper-concepts core).
///
/// @param gen  A uniform random bit generator (e.g. `std::mt19937`).
/// @param v0   First vertex.
/// @param rest Remaining N vertices.
/// @returns A `Vector<T, D>` holding the sampled point.
template <std::uniform_random_bit_generator RNG,
          ::zipper::concepts::Vector V,
          ::zipper::concepts::Vector... Rest>
    requires(std::same_as<V, Rest> && ...)
auto uniformly_sample_simplex(RNG &gen, const V &v0, const Rest &...rest)
    -> ::zipper::Vector<typename V::value_type,
                        V::extents_type::static_extent(0)> {
    using T = typename V::value_type;
    constexpr auto D = V::extents_type::static_extent(0);
    constexpr auto K = sizeof...(Rest); // N = K for an N-simplex with K+1 verts

    if constexpr (K == 0) {
        // 0-simplex (single vertex): the only sample is the vertex itself.
        return ::zipper::Vector<T, D>(v0);
    } else {
        std::uniform_real_distribution<T> dist(T(0), T(1));

        // Generate K random numbers and sort them.
        std::array<T, K> sorted;
        for (std::size_t i = 0; i < K; ++i) { sorted[i] = dist(gen); }
        std::sort(sorted.begin(), sorted.end());

        // Barycentric coordinates from differences:
        //   lambda_0 = sorted[0]
        //   lambda_i = sorted[i] - sorted[i-1]   for 1 <= i < K
        //   lambda_K = 1 - sorted[K-1]
        ::zipper::Vector<T, D> result = v0 * sorted[0];

        // Fold over the rest pack, accumulating from index 1..K-1 then K.
        std::size_t idx = 1;
        auto accumulate = [&](const auto &vi) {
            if (idx < K) {
                result += vi * (sorted[idx] - sorted[idx - 1]);
            } else {
                result += vi * (T(1) - sorted[K - 1]);
            }
            ++idx;
        };
        (accumulate(rest), ...);

        return result;
    }
}

// ============================================================================
// Layer 1.5: PointSpan wrappers
// ============================================================================

/// Uniformly sample a point from an N-simplex (PointSpan wrapper).
template <std::uniform_random_bit_generator RNG, PointSpan S, PointSpan... Rest>
    requires(std::same_as<S, Rest> && ...)
auto uniformly_sample_simplex(RNG &gen, S v0, Rest... rest)
    -> std::array<scalar_t<S>, dim_v<S>> {
    auto P = uniformly_sample_simplex(
        gen, ::zipper::VectorBase(v0), ::zipper::VectorBase(rest)...);
    return to_array(P);
}

// ============================================================================
// Layer 1.5M: Matrix wrappers
// ============================================================================

/// Uniformly sample a random point from an N-simplex given as a D×(N+1)
/// matrix of vertex columns.
///
/// Supports both static and dynamic column counts.
template <std::uniform_random_bit_generator RNG, ::zipper::concepts::Matrix M>
auto uniformly_sample_simplex(RNG &gen, const M &pts)
    -> ::zipper::Vector<typename std::decay_t<M>::value_type,
                        std::decay_t<M>::extents_type::static_extent(0)> {
    using T = typename std::decay_t<M>::value_type;
    constexpr auto D = std::decay_t<M>::extents_type::static_extent(0);

    const auto num_vertices = pts.extent(1); // N+1
    const auto N = num_vertices - 1;

    if (N == 0) {
        // 0-simplex: return the single vertex.
        return ::zipper::Vector<T, D>(pts.col(std::size_t{0}));
    }

    std::uniform_real_distribution<T> dist(T(0), T(1));

    // Generate N random numbers and sort them.
    // Use a small-buffer-optimised approach: stack array for small N,
    // heap for large/dynamic N.
    constexpr auto Nstatic = std::decay_t<M>::extents_type::static_extent(1);
    constexpr auto Kstatic =
        (Nstatic == std::dynamic_extent) ? std::dynamic_extent : Nstatic - 1;

    ::zipper::Vector<T, Kstatic> sorted(N);
    for (std::size_t i = 0; i < N; ++i) { sorted(i) = dist(gen); }
    std::sort(&sorted(std::size_t{0}), &sorted(std::size_t{0}) + N);

    // Barycentric coordinates from differences.
    ::zipper::Vector<T, D> result(pts.extent(0));
    result = pts.col(std::size_t{0}) * sorted(std::size_t{0});
    for (std::size_t i = 1; i < N; ++i) {
        result += pts.col(i) * (sorted(i) - sorted(i - 1));
    }
    result += pts.col(N) * (T(1) - sorted(N - 1));

    return result;
}

/// Sample multiple uniform random points from an N-simplex given as a
/// D×(N+1) matrix of vertex columns.
///
/// @returns A D×num_samples matrix of column-vector sample points.
template <std::uniform_random_bit_generator RNG, ::zipper::concepts::Matrix M>
auto uniformly_sample_simplex(RNG &gen, const M &pts, std::size_t num_samples)
    -> ::zipper::Matrix<typename std::decay_t<M>::value_type,
                        std::decay_t<M>::extents_type::static_extent(0),
                        std::dynamic_extent> {
    using T = typename std::decay_t<M>::value_type;
    constexpr auto D = std::decay_t<M>::extents_type::static_extent(0);

    ::zipper::Matrix<T, D, std::dynamic_extent> result(pts.extent(0),
                                                       num_samples);
    for (std::size_t i = 0; i < num_samples; ++i) {
        result.col(i) = uniformly_sample_simplex(gen, pts);
    }
    return result;
}

// ============================================================================
// Layer 1.5ID: HasAsTuple wrappers (IncidentData and similar)
// ============================================================================

/// Uniformly sample a point from an N-simplex (HasAsTuple wrapper).
template <std::uniform_random_bit_generator RNG, HasAsTuple ID>
auto uniformly_sample_simplex(RNG &gen, const ID &id) {
    return std::apply(
        [&gen](const auto &...vs) {
            auto P = uniformly_sample_simplex(gen, ::zipper::VectorBase(vs)...);
            return to_array(P);
        },
        id.as_tuple());
}

} // namespace balsa::geometry::simplex
#endif
