#if !defined(BALSA_GEOMETRY_SIMPLEX_EDGE_MATRIX_HPP)
#define BALSA_GEOMETRY_SIMPLEX_EDGE_MATRIX_HPP

/// @file edge_matrix.hpp
/// @brief Edge matrix construction for simplices.
///
/// The edge matrix E is the D×K matrix whose columns are edge vectors
/// emanating from the first vertex: E = [v1-v0 | v2-v0 | ... | vK-v0].
///
/// This is the most fundamental geometric quantity — volume, Jacobian,
/// circumcenter, and all quality metrics can be expressed in terms of E.
///
/// Four-layer design:
///   - **Layer 1 (zipper-concepts core)**: constrained on
///   `::zipper::concepts::Vector`.
///   - **Layer 1.5 (PointSpan wrappers)**: thin overloads converting spans.
///   - **Layer 1.5M (Matrix wrappers)**: overloads accepting a D×N matrix
///     of vertex columns.
///   - **Layer 1.5ID (HasAsTuple wrappers)**: for IncidentData and similar.

#include <balsa/geometry/simplex/concepts.hpp>

#include <cstddef>
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

/// Edge matrix of an N-simplex (zipper-concepts core).
///
/// Given K+1 vertex vectors of dimension D, returns the D×K matrix
/// E = [v1-v0 | v2-v0 | ... | vK-v0].
///
/// @return A `::zipper::Matrix<T, D, K>` edge matrix.
template <::zipper::concepts::Vector V, ::zipper::concepts::Vector... Rest>
    requires(sizeof...(Rest) >= 1) && (std::same_as<V, Rest> && ...)
auto edge_matrix(const V &v0, const Rest &...rest)
    -> ::zipper::Matrix<typename V::value_type,
                        V::extents_type::static_extent(0),
                        sizeof...(Rest)> {
    using T = typename V::value_type;
    constexpr auto D = V::extents_type::static_extent(0);
    constexpr auto K = sizeof...(Rest);

    ::zipper::Matrix<T, D, K> E;
    std::size_t col = 0;
    auto fill = [&](const auto &v) {
        E.col(col) = v - v0;
        ++col;
    };
    (fill(rest), ...);

    return E;
}

// ============================================================================
// Layer 1.5: PointSpan wrappers
// ============================================================================

/// Edge matrix of an N-simplex (PointSpan wrapper).
template <PointSpan S, PointSpan... Rest>
    requires(sizeof...(Rest) >= 1) && (std::same_as<S, Rest> && ...)
auto edge_matrix(S v0, Rest... rest) {
    return edge_matrix(::zipper::VectorBase(v0), ::zipper::VectorBase(rest)...);
}

// ============================================================================
// Layer 1.5M: Matrix wrappers
// ============================================================================

/// Edge matrix from a D×N matrix of vertex columns.
///
/// Returns a D×(N-1) edge matrix E where col i = pts.col(i+1) - pts.col(0).
/// Supports both static and dynamic column counts.
template <::zipper::concepts::Matrix M>
auto edge_matrix(const M &pts) {
    using T = typename std::decay_t<M>::value_type;
    constexpr auto D = std::decay_t<M>::extents_type::static_extent(0);
    constexpr auto N = std::decay_t<M>::extents_type::static_extent(1);
    constexpr auto K = (N == std::dynamic_extent) ? std::dynamic_extent : N - 1;

    const auto n = pts.extent(1);
    const auto k = n - 1;

    ::zipper::Matrix<T, D, K> E(pts.extent(0), k);
    auto v0 = pts.col(std::size_t{0});
    for (std::size_t i = 0; i < k; ++i) { E.col(i) = pts.col(i + 1) - v0; }

    return E;
}

// ============================================================================
// Layer 1.5ID: HasAsTuple wrappers (IncidentData and similar)
// ============================================================================

/// Edge matrix of an N-simplex (HasAsTuple wrapper).
template <HasAsTuple ID>
auto edge_matrix(const ID &id) {
    return std::apply(
        [](const auto &...vs) {
            return edge_matrix(::zipper::VectorBase(vs)...);
        },
        id.as_tuple());
}

} // namespace balsa::geometry::simplex
#endif
