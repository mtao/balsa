#if !defined(BALSA_GEOMETRY_SIMPLEX_CIRCUMCENTER_HPP)
#define BALSA_GEOMETRY_SIMPLEX_CIRCUMCENTER_HPP

/// @file circumcenter.hpp
/// @brief Circumcenter computation for simplices.
///
/// Three-layer design:
///   - **Layer 1 (zipper-concepts core)**: vertex-vector API constrained
///     on `::zipper::concepts::Vector`.
///   - **Layer 1.5 (PointSpan wrappers)**: thin overloads converting
///     `std::span<const T, N>` to zipper via `VectorBase(span)` CTAD.
///   - **Layer 1.5M (Matrix wrappers)**: overloads accepting a D×N
///     matrix of vertex columns.
///
/// Both Layer 1 and Layer 1.5M build a D×K edge matrix (K = N-1) and
/// delegate to `detail::circumcenter_from_edges()` for the shared
/// Gram-system solve:
///   1. Gram matrix G = E^T * E  (K×K, SPD for non-degenerate simplices).
///   2. rhs_i = G(i,i) / 2.
///   3. LDLT-solve G * y = rhs.
///   4. Circumcenter = v0 + E * y.

#include <balsa/geometry/simplex/concepts.hpp>
#include <balsa/geometry/simplex/detail/circumcenter_solve.hpp>

#include <array>
#include <cmath>
#include <cstddef>
#include <span>
#include <tuple>
#include <utility>

#include <zipper/Matrix.hpp>
#include <zipper/Vector.hpp>
#include <zipper/VectorBase.hpp>
#include <zipper/concepts/Matrix.hpp>
#include <zipper/concepts/Vector.hpp>

#include "balsa/eigen/concepts/matrix_types.hpp"
#include "balsa/eigen/zipper_compat.hpp"

namespace balsa::geometry::simplex {

// ============================================================================
// Layer 1: zipper-concepts core
// ============================================================================

/// Circumcenter of a simplex given as individual vertex vectors (zipper core).
///
/// Accepts 2 or more vertex vectors of dimension D.  Returns a `Vector<T, D>`
/// holding the circumcenter coordinates.
///
/// For 2 vertices (edge midpoint): returns (v0 + v1) / 2.
/// For K+1 vertices: builds edge matrix E and delegates to
/// `detail::circumcenter_from_edges()`.
///
/// On degenerate input (singular Gram matrix), falls back to returning
/// the centroid of the vertices.
template <::zipper::concepts::Vector V, ::zipper::concepts::Vector... Rest>
    requires(std::same_as<V, Rest> && ...)
auto circumcenter(const V &v0, const Rest &...rest)
    -> ::zipper::Vector<typename V::value_type,
                        V::extents_type::static_extent(0)> {
    using T = typename V::value_type;
    constexpr auto D = V::extents_type::static_extent(0);
    constexpr auto K = sizeof...(Rest); // number of edge vectors

    if constexpr (K == 0) {
        // Single vertex: circumcenter is the vertex itself.
        return ::zipper::Vector<T, D>(v0);
    } else if constexpr (K == 1) {
        // Two vertices: midpoint.
        auto midpoint = [&](const auto &v1) { return (v0 + v1) / T{2}; };
        return midpoint(rest...);
    } else {
        // K+1 vertices, K >= 2 edge vectors.
        // Build D×K edge matrix E where col i = v_{i+1} - v0.
        ::zipper::Matrix<T, D, K> E;
        std::size_t col = 0;
        auto fill_edge = [&](const auto &v) {
            E.col(col) = v - v0;
            ++col;
        };
        (fill_edge(rest), ...);

        // Compute centroid as fallback for degenerate cases.
        ::zipper::Vector<T, D> centroid(v0);
        auto add_vertex = [&](const auto &v) { centroid += v; };
        (add_vertex(rest), ...);
        centroid /= T(K + 1);

        return detail::circumcenter_from_edges(v0, E, centroid);
    }
}

/// Circumcenter and squared circumradius (zipper-concepts core).
template <::zipper::concepts::Vector V, ::zipper::concepts::Vector... Rest>
    requires(std::same_as<V, Rest> && ...)
auto circumcenter_with_squared_radius(const V &v0, const Rest &...rest) {
    auto C = circumcenter(v0, rest...);
    using T = typename V::value_type;
    constexpr auto D = V::extents_type::static_extent(0);
    ::zipper::Vector<T, D> diff = C - v0;
    T r2 = diff.template norm_powered<2>();
    return std::make_tuple(C, r2);
}

/// Circumcenter and circumradius (zipper-concepts core).
template <::zipper::concepts::Vector V, ::zipper::concepts::Vector... Rest>
    requires(std::same_as<V, Rest> && ...)
auto circumcenter_with_radius(const V &v0, const Rest &...rest) {
    auto [C, r2] = circumcenter_with_squared_radius(v0, rest...);
    return std::make_tuple(C, std::sqrt(r2));
}

// ============================================================================
// Layer 1.5: PointSpan wrappers
// ============================================================================

/// Circumcenter of a simplex (PointSpan wrapper).
template <PointSpan S, PointSpan... Rest>
    requires(std::same_as<S, Rest> && ...)
auto circumcenter(S v0, Rest... rest) -> std::array<scalar_t<S>, dim_v<S>> {
    auto C =
        circumcenter(::zipper::VectorBase(v0), ::zipper::VectorBase(rest)...);
    return to_array(C);
}

/// Circumcenter and squared circumradius (PointSpan wrapper).
template <PointSpan S, PointSpan... Rest>
    requires(std::same_as<S, Rest> && ...)
auto circumcenter_with_squared_radius(S v0, Rest... rest) {
    auto [C, r2] = circumcenter_with_squared_radius(
        ::zipper::VectorBase(v0), ::zipper::VectorBase(rest)...);
    return std::make_tuple(to_array(C), r2);
}

/// Circumcenter and circumradius (PointSpan wrapper).
template <PointSpan S, PointSpan... Rest>
    requires(std::same_as<S, Rest> && ...)
auto circumcenter_with_radius(S v0, Rest... rest) {
    auto [C, r] = circumcenter_with_radius(::zipper::VectorBase(v0),
                                           ::zipper::VectorBase(rest)...);
    return std::make_tuple(to_array(C), r);
}

// ============================================================================
// Layer 1.5M: Matrix wrappers
// ============================================================================

/// Circumcenter of a simplex from a D×N matrix of vertex columns.
///
/// Builds the D×K edge matrix (K = N-1) from column differences and
/// delegates to `detail::circumcenter_from_edges()`.
/// Supports both static and dynamic column counts.
template <::zipper::concepts::Matrix M>
auto circumcenter(const M &pts) {
    using T = typename std::decay_t<M>::value_type;
    constexpr auto D = std::decay_t<M>::extents_type::static_extent(0);
    constexpr auto N = std::decay_t<M>::extents_type::static_extent(1);
    // K = number of edges = N - 1 (may be dynamic_extent)
    constexpr auto K = (N == std::dynamic_extent) ? std::dynamic_extent : N - 1;

    const auto n = pts.extent(1); // runtime column count

    if (n == 1) {
        // Single vertex.
        return ::zipper::Vector<T, D>(pts.col(std::size_t{0}));
    }

    if (n == 2) {
        // Two vertices: midpoint.
        return ((pts.col(std::size_t{0}) + pts.col(std::size_t{1})) / T{2})
            .eval();
    }

    // Build D×K edge matrix E where col i = pts.col(i+1) - pts.col(0).
    const auto k = n - 1;
    ::zipper::Matrix<T, D, K> E(pts.extent(0), k);
    auto v0 = pts.col(std::size_t{0});
    for (std::size_t i = 0; i < k; ++i) { E.col(i) = pts.col(i + 1) - v0; }

    // Compute centroid as fallback for degenerate cases.
    ::zipper::Vector<T, D> centroid(pts.extent(0));
    centroid = pts.col(std::size_t{0});
    for (std::size_t i = 1; i < n; ++i) { centroid += pts.col(i); }
    centroid /= T(n);

    return detail::circumcenter_from_edges(v0, E, centroid);
}

/// Circumcenter and squared circumradius from a D×N matrix of vertex columns.
template <::zipper::concepts::Matrix M>
auto circumcenter_with_squared_radius(const M &pts) {
    using T = typename std::decay_t<M>::value_type;
    constexpr auto D = std::decay_t<M>::extents_type::static_extent(0);
    auto C = circumcenter(pts);
    auto v0 = pts.col(std::size_t{0});
    ::zipper::Vector<T, D> diff = C - v0;
    T r2 = diff.template norm_powered<2>();
    return std::make_tuple(std::move(C), r2);
}

/// Circumcenter and circumradius from a D×N matrix of vertex columns.
template <::zipper::concepts::Matrix M>
auto circumcenter_with_radius(const M &pts) {
    auto [C, r2] = circumcenter_with_squared_radius(pts);
    return std::make_tuple(std::move(C), std::sqrt(r2));
}

// ============================================================================
// Layer 1.5ID: HasAsTuple wrappers (IncidentData and similar)
// ============================================================================

/// Circumcenter of a simplex (HasAsTuple wrapper).
///
/// Accepts any type providing `.as_tuple()` whose elements are convertible
/// to zipper vectors via `VectorBase` CTAD (e.g. `IncidentData`).
template <HasAsTuple ID>
auto circumcenter(const ID &id) {
    return std::apply(
        [](const auto &...vs) {
            auto C = circumcenter(::zipper::VectorBase(vs)...);
            return to_array(C);
        },
        id.as_tuple());
}

/// Circumcenter and squared circumradius (HasAsTuple wrapper).
template <HasAsTuple ID>
auto circumcenter_with_squared_radius(const ID &id) {
    return std::apply(
        [](const auto &...vs) {
            auto [C, r2] =
                circumcenter_with_squared_radius(::zipper::VectorBase(vs)...);
            return std::make_pair(to_array(C), r2);
        },
        id.as_tuple());
}

/// Circumcenter and circumradius (HasAsTuple wrapper).
template <HasAsTuple ID>
auto circumcenter_with_radius(const ID &id) {
    return std::apply(
        [](const auto &...vs) {
            auto [C, r] = circumcenter_with_radius(::zipper::VectorBase(vs)...);
            return std::make_pair(to_array(C), r);
        },
        id.as_tuple());
}

// ── Eigen overloads (convert to zipper) ─────────────────────────────

template <eigen::concepts::MatrixBaseDerived M>
auto circumcenter(const M &V) {
    return circumcenter(eigen::as_zipper(V));
}

template <eigen::concepts::MatrixBaseDerived M>
auto circumcenter_with_squared_radius(const M &V) {
    return circumcenter_with_squared_radius(eigen::as_zipper(V));
}

template <eigen::concepts::MatrixBaseDerived M>
auto circumcenter_with_radius(const M &V) {
    return circumcenter_with_radius(eigen::as_zipper(V));
}

} // namespace balsa::geometry::simplex
#endif
