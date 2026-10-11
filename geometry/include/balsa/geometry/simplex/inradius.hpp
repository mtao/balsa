#if !defined(BALSA_GEOMETRY_SIMPLEX_INRADIUS_HPP)
#define BALSA_GEOMETRY_SIMPLEX_INRADIUS_HPP

/// @file inradius.hpp
/// @brief Inradius computation for simplices.
///
/// `inradius(v0, v1, ...)` computes the inradius of a K-simplex as:
///   inradius = K * volume_unsigned / boundary_volume
///
/// This is the generic formula:
///   - Triangle (K=2): 2 * area / perimeter = area / semi_perimeter
///   - Tetrahedron (K=3): 3 * volume / surface_area
///
/// Returns 0 for degenerate simplices (zero boundary volume).
/// Requires at least 3 vertices (K >= 2).
///
/// Four-layer design:
///   - **Layer 1 (zipper-concepts core)**: constrained on
///   `::zipper::concepts::Vector`.
///   - **Layer 1.5 (PointSpan wrappers)**: thin overloads converting spans.
///   - **Layer 1.5M (Matrix wrappers)**: overloads accepting a D×N matrix.
///   - **Layer 1.5ID (HasAsTuple wrappers)**: for IncidentData and similar.

#include <balsa/geometry/simplex/boundary_volume.hpp>
#include <balsa/geometry/simplex/concepts.hpp>
#include <balsa/geometry/simplex/volume.hpp>

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

/// Inradius of a K-simplex: K * volume / boundary_volume (zipper core).
///
/// Requires at least 3 vertices (K >= 2).
/// Returns 0 for degenerate simplices.
template <::zipper::concepts::Vector V, ::zipper::concepts::Vector... Rest>
    requires(sizeof...(Rest) >= 2) && (std::same_as<V, Rest> && ...)
auto inradius(const V &v0, const Rest &...rest) -> typename V::value_type {
    using T = typename V::value_type;
    constexpr auto K = sizeof...(Rest); // simplex dimension

    T vol = volume_unsigned(v0, rest...);
    T bvol = boundary_volume(v0, rest...);

    if (bvol <= T{0}) return T{0};
    return T(K) * vol / bvol;
}

// ============================================================================
// Layer 1.5: PointSpan wrappers
// ============================================================================

/// Inradius of a K-simplex (PointSpan wrapper).
template <PointSpan S, PointSpan... Rest>
    requires(sizeof...(Rest) >= 2) && (std::same_as<S, Rest> && ...)
auto inradius(S v0, Rest... rest) -> scalar_t<S> {
    return inradius(::zipper::VectorBase(v0), ::zipper::VectorBase(rest)...);
}

// ============================================================================
// Layer 1.5M: Matrix wrappers
// ============================================================================

/// Inradius from a D×N matrix of vertex columns (N >= 3).
template <::zipper::concepts::Matrix M>
    requires(std::decay_t<M>::extents_type::static_extent(1)
             != std::dynamic_extent)
            && (std::decay_t<M>::extents_type::static_extent(1) >= 3)
auto inradius(const M &pts) -> typename std::decay_t<M>::value_type {
    using T = typename std::decay_t<M>::value_type;
    constexpr auto N = std::decay_t<M>::extents_type::static_extent(1);
    constexpr auto K = N - 1; // simplex dimension

    T vol = volume_unsigned(pts);
    T bvol = boundary_volume(pts);

    if (bvol <= T{0}) return T{0};
    return T(K) * vol / bvol;
}

// ============================================================================
// Layer 1.5ID: HasAsTuple wrappers (IncidentData and similar)
// ============================================================================

/// Inradius of a simplex (HasAsTuple wrapper).
template <HasAsTuple ID>
auto inradius(const ID &id) {
    return std::apply(
        [](const auto &...vs) { return inradius(::zipper::VectorBase(vs)...); },
        id.as_tuple());
}

} // namespace balsa::geometry::simplex
#endif
