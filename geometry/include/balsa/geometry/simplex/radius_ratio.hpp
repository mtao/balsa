#if !defined(BALSA_GEOMETRY_SIMPLEX_RADIUS_RATIO_HPP)
#define BALSA_GEOMETRY_SIMPLEX_RADIUS_RATIO_HPP

/// @file radius_ratio.hpp
/// @brief Radius ratio quality metric for simplices.
///
/// Generic: radius_ratio = K * inradius / circumradius
///   - Triangle (K=2): 2r/R, range [0, 1], ideal = 1 (equilateral).
///   - Tet (K=3): 3r/R, range [0, 1], ideal = 1 (regular).
///
/// Four-layer design:
///   - **Layer 1 (zipper-concepts core)**: constrained on
///   `::zipper::concepts::Vector`.
///   - **Layer 1.5 (PointSpan wrappers)**: thin overloads converting spans.
///   - **Layer 1.5M (Matrix wrappers)**: overloads accepting a D×N matrix.
///   - **Layer 1.5ID (HasAsTuple wrappers)**: for IncidentData and similar.

#include <balsa/geometry/simplex/circumradius.hpp>
#include <balsa/geometry/simplex/concepts.hpp>
#include <balsa/geometry/simplex/inradius.hpp>

#include <algorithm>
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

/// Radius ratio of a K-simplex: K * inradius / circumradius (zipper core).
///
/// Range [0, 1], ideal = 1 for regular simplices.
/// Returns 0 for degenerate simplices.
template <::zipper::concepts::Vector V, ::zipper::concepts::Vector... Rest>
    requires(sizeof...(Rest) >= 2) && (std::same_as<V, Rest> && ...)
auto radius_ratio(const V &v0, const Rest &...rest) -> typename V::value_type {
    using T = typename V::value_type;
    constexpr auto K = sizeof...(Rest);

    T r = inradius(v0, rest...);
    T R = circumradius(v0, rest...);

    if (R <= T{0}) return T{0};
    return std::clamp(T(K) * r / R, T{0}, T{1});
}

// ============================================================================
// Layer 1.5: PointSpan wrappers
// ============================================================================

/// Radius ratio of a K-simplex (PointSpan wrapper).
template <PointSpan S, PointSpan... Rest>
    requires(sizeof...(Rest) >= 2) && (std::same_as<S, Rest> && ...)
auto radius_ratio(S v0, Rest... rest) -> scalar_t<S> {
    return radius_ratio(::zipper::VectorBase(v0),
                        ::zipper::VectorBase(rest)...);
}

// ============================================================================
// Layer 1.5M: Matrix wrappers
// ============================================================================

/// Radius ratio from a D×N matrix of vertex columns (N >= 3).
template <::zipper::concepts::Matrix M>
    requires(std::decay_t<M>::extents_type::static_extent(1)
             != std::dynamic_extent)
            && (std::decay_t<M>::extents_type::static_extent(1) >= 3)
auto radius_ratio(const M &pts) -> typename std::decay_t<M>::value_type {
    using T = typename std::decay_t<M>::value_type;
    constexpr auto N = std::decay_t<M>::extents_type::static_extent(1);
    constexpr auto K = N - 1;

    T r = inradius(pts);
    T R = circumradius(pts);

    if (R <= T{0}) return T{0};
    return std::clamp(T(K) * r / R, T{0}, T{1});
}

// ============================================================================
// Layer 1.5ID: HasAsTuple wrappers (IncidentData and similar)
// ============================================================================

/// Radius ratio of a simplex (HasAsTuple wrapper).
template <HasAsTuple ID>
auto radius_ratio(const ID &id) {
    return std::apply(
        [](const auto &...vs) {
            return radius_ratio(::zipper::VectorBase(vs)...);
        },
        id.as_tuple());
}

} // namespace balsa::geometry::simplex
#endif
