#if !defined(BALSA_GEOMETRY_SIMPLEX_CIRCUMRADIUS_HPP)
#define BALSA_GEOMETRY_SIMPLEX_CIRCUMRADIUS_HPP

/// @file circumradius.hpp
/// @brief Circumradius computation for simplices.
///
/// Thin wrappers around `circumcenter_with_radius` and
/// `circumcenter_with_squared_radius` that discard the center and
/// return only the radius.
///
/// Four-layer design:
///   - **Layer 1 (zipper-concepts core)**: constrained on
///   `::zipper::concepts::Vector`.
///   - **Layer 1.5 (PointSpan wrappers)**: thin overloads converting spans.
///   - **Layer 1.5M (Matrix wrappers)**: overloads accepting a D×N matrix.
///   - **Layer 1.5ID (HasAsTuple wrappers)**: for IncidentData and similar.

#include <balsa/geometry/simplex/circumcenter.hpp>

#include <cstddef>
#include <span>
#include <tuple>
#include <utility>

#include <zipper/VectorBase.hpp>
#include <zipper/concepts/Matrix.hpp>
#include <zipper/concepts/Vector.hpp>

namespace balsa::geometry::simplex {

// ============================================================================
// Layer 1: zipper-concepts core
// ============================================================================

/// Squared circumradius of a simplex (zipper core).
template <::zipper::concepts::Vector V, ::zipper::concepts::Vector... Rest>
    requires(std::same_as<V, Rest> && ...)
auto circumradius_squared(const V &v0, const Rest &...rest) ->
    typename V::value_type {
    auto [C, r2] = circumcenter_with_squared_radius(v0, rest...);
    return r2;
}

/// Circumradius of a simplex (zipper core).
template <::zipper::concepts::Vector V, ::zipper::concepts::Vector... Rest>
    requires(std::same_as<V, Rest> && ...)
auto circumradius(const V &v0, const Rest &...rest) -> typename V::value_type {
    auto [C, r] = circumcenter_with_radius(v0, rest...);
    return r;
}

// ============================================================================
// Layer 1.5: PointSpan wrappers
// ============================================================================

/// Squared circumradius of a simplex (PointSpan wrapper).
template <PointSpan S, PointSpan... Rest>
    requires(std::same_as<S, Rest> && ...)
auto circumradius_squared(S v0, Rest... rest) -> scalar_t<S> {
    return circumradius_squared(::zipper::VectorBase(v0),
                                ::zipper::VectorBase(rest)...);
}

/// Circumradius of a simplex (PointSpan wrapper).
template <PointSpan S, PointSpan... Rest>
    requires(std::same_as<S, Rest> && ...)
auto circumradius(S v0, Rest... rest) -> scalar_t<S> {
    return circumradius(::zipper::VectorBase(v0),
                        ::zipper::VectorBase(rest)...);
}

// ============================================================================
// Layer 1.5M: Matrix wrappers
// ============================================================================

/// Squared circumradius from a D×N matrix of vertex columns.
template <::zipper::concepts::Matrix M>
auto circumradius_squared(const M &pts) ->
    typename std::decay_t<M>::value_type {
    auto [C, r2] = circumcenter_with_squared_radius(pts);
    return r2;
}

/// Circumradius from a D×N matrix of vertex columns.
template <::zipper::concepts::Matrix M>
auto circumradius(const M &pts) -> typename std::decay_t<M>::value_type {
    auto [C, r] = circumcenter_with_radius(pts);
    return r;
}

// ============================================================================
// Layer 1.5ID: HasAsTuple wrappers (IncidentData and similar)
// ============================================================================

/// Squared circumradius of a simplex (HasAsTuple wrapper).
template <HasAsTuple ID>
auto circumradius_squared(const ID &id) {
    return std::apply(
        [](const auto &...vs) {
            return circumradius_squared(::zipper::VectorBase(vs)...);
        },
        id.as_tuple());
}

/// Circumradius of a simplex (HasAsTuple wrapper).
template <HasAsTuple ID>
auto circumradius(const ID &id) {
    return std::apply(
        [](const auto &...vs) {
            return circumradius(::zipper::VectorBase(vs)...);
        },
        id.as_tuple());
}

} // namespace balsa::geometry::simplex
#endif
