#if !defined(BALSA_GEOMETRY_SIMPLEX_VOLUME_HPP)
#define BALSA_GEOMETRY_SIMPLEX_VOLUME_HPP

/// @file volume.hpp
/// @brief Signed and unsigned volume of simplices.
///
/// Four-layer design:
///   - **Layer 1 (zipper-concepts core)**: constrained on
///   `::zipper::concepts::Vector`.
///   - **Layer 1.5 (PointSpan wrappers)**: thin overloads that convert
///     `std::span<const T, N>` to zipper via `VectorBase(span)` CTAD and
///     delegate.
///   - **Layer 1.5M (Matrix wrappers)**: overloads accepting a D×N
///     matrix of vertex columns.
///   - **Layer 1.5ID (HasAsTuple wrappers)**: for IncidentData and similar.
///
/// All functions build a D×K edge matrix via `edge_matrix()` and delegate to
/// `detail::volume_signed_from_edges()` /
/// `detail::volume_unsigned_from_edges()` for the shared determinant logic.

#include <balsa/geometry/simplex/concepts.hpp>
#include <balsa/geometry/simplex/detail/volume_from_edges.hpp>
#include <balsa/geometry/simplex/edge_matrix.hpp>

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

namespace balsa::geometry::simplex {

// ============================================================================
// Layer 1: zipper-concepts core
// ============================================================================

/// Signed volume of an N-simplex (zipper-concepts core).
///
/// Builds a D×K edge matrix and delegates to
/// `detail::volume_signed_from_edges()`.
/// Requires D == K (i.e. D+1 vertices in D dimensions).
template <::zipper::concepts::Vector V, ::zipper::concepts::Vector... Rest>
    requires(V::extents_type::static_extent(0) == sizeof...(Rest))
            && (std::same_as<V, Rest> && ...)
auto volume_signed(const V &v0, const Rest &...rest) -> typename V::value_type {
    constexpr auto D = V::extents_type::static_extent(0);
    constexpr auto K = sizeof...(Rest);
    static_assert(K == D);

    auto E = edge_matrix(v0, rest...);
    return detail::volume_signed_from_edges(E);
}

/// Unsigned volume of an N-simplex (zipper-concepts core).
///
/// Builds a D×K edge matrix and delegates to
/// `detail::volume_unsigned_from_edges()`.
template <::zipper::concepts::Vector V, ::zipper::concepts::Vector... Rest>
    requires(V::extents_type::static_extent(0) >= sizeof...(Rest))
            && (std::same_as<V, Rest> && ...)
auto volume_unsigned(const V &v0, const Rest &...rest) ->
    typename V::value_type {
    auto E = edge_matrix(v0, rest...);
    return detail::volume_unsigned_from_edges(E);
}

// ============================================================================
// Layer 1.5: PointSpan wrappers
// ============================================================================

/// Signed volume of an N-simplex (PointSpan wrapper).
template <PointSpan S, PointSpan... Rest>
    requires(dim_v<S> == sizeof...(Rest)) && (std::same_as<S, Rest> && ...)
auto volume_signed(S v0, Rest... rest) -> scalar_t<S> {
    return volume_signed(::zipper::VectorBase(v0),
                         ::zipper::VectorBase(rest)...);
}

/// Unsigned volume of an N-simplex (PointSpan wrapper).
template <PointSpan S, PointSpan... Rest>
    requires(dim_v<S> >= sizeof...(Rest)) && (std::same_as<S, Rest> && ...)
auto volume_unsigned(S v0, Rest... rest) -> scalar_t<S> {
    return volume_unsigned(::zipper::VectorBase(v0),
                           ::zipper::VectorBase(rest)...);
}

// ============================================================================
// Layer 1.5M: Matrix wrappers
// ============================================================================

/// Signed volume of an N-simplex from a D×N matrix of vertex columns.
///
/// Builds a D×K edge matrix (K = N-1) via `edge_matrix()` and delegates
/// to `detail::volume_signed_from_edges()`.
/// Supports both static and dynamic column counts.
template <::zipper::concepts::Matrix M>
auto volume_signed(const M &pts) -> typename std::decay_t<M>::value_type {
    auto E = edge_matrix(pts);
    return detail::volume_signed_from_edges(E);
}

/// Unsigned volume of an N-simplex from a D×N matrix of vertex columns.
///
/// Builds a D×K edge matrix (K = N-1) via `edge_matrix()` and delegates
/// to `detail::volume_unsigned_from_edges()`.
/// Supports both static and dynamic column counts.
template <::zipper::concepts::Matrix M>
auto volume_unsigned(const M &pts) -> typename std::decay_t<M>::value_type {
    auto E = edge_matrix(pts);
    return detail::volume_unsigned_from_edges(E);
}

// ============================================================================
// Layer 1.5ID: HasAsTuple wrappers (IncidentData and similar)
// ============================================================================

/// Signed volume of an N-simplex (HasAsTuple wrapper).
///
/// Accepts any type providing `.as_tuple()` whose elements are convertible
/// to zipper vectors via `VectorBase` CTAD (e.g. `IncidentData`).
template <HasAsTuple ID>
auto volume_signed(const ID &id) {
    return std::apply(
        [](const auto &...vs) {
            return volume_signed(::zipper::VectorBase(vs)...);
        },
        id.as_tuple());
}

/// Unsigned volume of an N-simplex (HasAsTuple wrapper).
template <HasAsTuple ID>
auto volume_unsigned(const ID &id) {
    return std::apply(
        [](const auto &...vs) {
            return volume_unsigned(::zipper::VectorBase(vs)...);
        },
        id.as_tuple());
}

// ── Convenience: signed for full-rank simplices, unsigned otherwise ──

template <::zipper::concepts::Matrix M>
auto volume(const M &V) -> typename M::value_type {
    if (V.extent(0) + 1 == V.extent(1)) { return volume_signed(V); }
    return volume_unsigned(V);
}

} // namespace balsa::geometry::simplex
#endif
