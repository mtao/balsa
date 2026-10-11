#if !defined(BALSA_GEOMETRY_SIMPLEX_JACOBIAN_HPP)
#define BALSA_GEOMETRY_SIMPLEX_JACOBIAN_HPP

/// @file jacobian.hpp
/// @brief Jacobian computation for simplices relative to a reference element.
///
/// The Jacobian J = E * inv(R) maps from the reference simplex to the
/// physical simplex, where E is the edge matrix and R is the reference
/// element's edge matrix.
///
/// For quality metrics, the reference is typically the regular (equilateral /
/// regular) simplex:
///   - K=2 (triangle): R = [[1, 0.5], [0, sqrt(3)/2]]
///     inv(R) = [[1, -1/sqrt(3)], [0, 2/sqrt(3)]]
///   - K=3 (tet): R = regular tet edge matrix
///     inv(R) = [[1, -1/sqrt(3), -1/sqrt(6)],
///               [0,  2/sqrt(3), -1/sqrt(6)],
///               [0,  0,          3/sqrt(6)]]
///
/// `reference_simplex_inverse<K>()` returns the K×K inverse of the reference
/// K-simplex edge matrix. Currently supports K=2 and K=3.
///
/// `jacobian(v0, v1, ...)` computes J = E * reference_simplex_inverse<K>()
/// using the regular simplex as reference.
///
/// `jacobian(E, ref_inv)` computes J = E * ref_inv for a custom reference
/// element (e.g., texture mapping distortion).
///
/// Four-layer design:
///   - **Layer 1 (zipper-concepts core)**: constrained on
///   `::zipper::concepts::Vector`.
///   - **Layer 1.5 (PointSpan wrappers)**: thin overloads converting spans.
///   - **Layer 1.5M (Matrix wrappers)**: overloads accepting a D×N matrix.
///   - **Layer 1.5ID (HasAsTuple wrappers)**: for IncidentData and similar.

#include <balsa/geometry/simplex/concepts.hpp>
#include <balsa/geometry/simplex/edge_matrix.hpp>

#include <cmath>
#include <cstddef>
#include <numbers>
#include <span>
#include <tuple>
#include <utility>

#include <zipper/Matrix.hpp>
#include <zipper/VectorBase.hpp>
#include <zipper/concepts/Matrix.hpp>
#include <zipper/concepts/Vector.hpp>

namespace balsa::geometry::simplex {

// ============================================================================
// Reference simplex inverse matrices
// ============================================================================

/// Inverse of the reference regular K-simplex edge matrix.
///
/// Returns a K×K matrix `inv(R)` such that J = E * inv(R) maps from the
/// regular K-simplex to the physical element.
///
/// Currently supports K=2 (equilateral triangle) and K=3 (regular tet).
template <std::size_t K, typename T = double>
auto reference_simplex_inverse() -> ::zipper::Matrix<T, K, K> {
    static_assert(K == 2 || K == 3,
                  "reference_simplex_inverse only supports K=2 and K=3");

    ::zipper::Matrix<T, K, K> inv_R;

    if constexpr (K == 2) {
        // Reference equilateral triangle:
        //   R = [[1, 0.5], [0, sqrt(3)/2]]
        //   inv(R) = [[1, -1/sqrt(3)], [0, 2/sqrt(3)]]
        const T inv_sqrt3 = T{1} / std::numbers::sqrt3_v<T>;
        const T two_inv_sqrt3 = T{2} * inv_sqrt3;

        inv_R(0, 0) = T{1};
        inv_R(0, 1) = -inv_sqrt3;
        inv_R(1, 0) = T{0};
        inv_R(1, 1) = two_inv_sqrt3;
    } else if constexpr (K == 3) {
        // Reference regular tetrahedron:
        //   inv(W) = [[1, -1/sqrt(3), -1/sqrt(6)],
        //             [0,  2/sqrt(3), -1/sqrt(6)],
        //             [0,  0,          3/sqrt(6)]]
        constexpr T sqrt3 = std::numbers::sqrt3_v<T>;
        constexpr T sqrt6 =
            T{2.44948974278317809819728407470589139196594748328};

        const T inv_sqrt3 = T{1} / sqrt3;
        const T inv_sqrt6 = T{1} / sqrt6;
        const T two_inv_sqrt3 = T{2} / sqrt3;
        const T three_inv_sqrt6 = T{3} / sqrt6;

        inv_R(0, 0) = T{1};
        inv_R(0, 1) = -inv_sqrt3;
        inv_R(0, 2) = -inv_sqrt6;
        inv_R(1, 0) = T{0};
        inv_R(1, 1) = two_inv_sqrt3;
        inv_R(1, 2) = -inv_sqrt6;
        inv_R(2, 0) = T{0};
        inv_R(2, 1) = T{0};
        inv_R(2, 2) = three_inv_sqrt6;
    }

    return inv_R;
}

// ============================================================================
// Custom reference Jacobian: J = E * ref_inv
// ============================================================================

/// Jacobian from a precomputed edge matrix and reference inverse.
///
/// J = E * ref_inv where E is D×K and ref_inv is K×K.
/// Returns a D×K matrix.
template <::zipper::concepts::Matrix EdgeMat, ::zipper::concepts::Matrix RefInv>
auto jacobian(const EdgeMat &E, const RefInv &ref_inv) {
    return (E * ref_inv).eval();
}

// ============================================================================
// Layer 1: zipper-concepts core
// ============================================================================

/// Jacobian relative to the regular reference simplex (zipper core).
///
/// Computes J = edge_matrix(v0, rest...) * reference_simplex_inverse<K>().
template <::zipper::concepts::Vector V, ::zipper::concepts::Vector... Rest>
    requires(sizeof...(Rest) >= 2) && (std::same_as<V, Rest> && ...)
auto jacobian(const V &v0, const Rest &...rest) {
    using T = typename V::value_type;
    constexpr auto K = sizeof...(Rest);

    auto E = edge_matrix(v0, rest...);
    auto inv_R = reference_simplex_inverse<K, T>();
    return (E * inv_R).eval();
}

// ============================================================================
// Layer 1.5: PointSpan wrappers
// ============================================================================

/// Jacobian relative to the regular reference simplex (PointSpan wrapper).
template <PointSpan S, PointSpan... Rest>
    requires(sizeof...(Rest) >= 2) && (std::same_as<S, Rest> && ...)
auto jacobian(S v0, Rest... rest) {
    return jacobian(::zipper::VectorBase(v0), ::zipper::VectorBase(rest)...);
}

// ============================================================================
// Layer 1.5M: Matrix wrappers
// ============================================================================

/// Jacobian from a D×N matrix of vertex columns (N >= 3, static extent).
template <::zipper::concepts::Matrix M>
    requires(std::decay_t<M>::extents_type::static_extent(1)
             != std::dynamic_extent)
            && (std::decay_t<M>::extents_type::static_extent(1) >= 3)
auto jacobian(const M &pts) {
    using T = typename std::decay_t<M>::value_type;
    constexpr auto N = std::decay_t<M>::extents_type::static_extent(1);
    constexpr auto K = N - 1;

    auto E = edge_matrix(pts);
    auto inv_R = reference_simplex_inverse<K, T>();
    return (E * inv_R).eval();
}

// ============================================================================
// Layer 1.5ID: HasAsTuple wrappers (IncidentData and similar)
// ============================================================================

/// Jacobian relative to the regular reference simplex (HasAsTuple wrapper).
template <HasAsTuple ID>
auto jacobian(const ID &id) {
    return std::apply(
        [](const auto &...vs) { return jacobian(::zipper::VectorBase(vs)...); },
        id.as_tuple());
}

} // namespace balsa::geometry::simplex
#endif
