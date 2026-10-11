#if !defined(BALSA_GEOMETRY_SIMPLEX_ASPECT_RATIO_HPP)
#define BALSA_GEOMETRY_SIMPLEX_ASPECT_RATIO_HPP

/// @file aspect_ratio.hpp
/// @brief Aspect ratio quality metric for simplices.
///
/// Generic: aspect_ratio = max_edge_length / (2 * C_k * inradius)
/// where C_k = sqrt(k*(k+1)/2).
///   - Triangle (K=2): C_2 = sqrt(3), range [1, inf), ideal = 1.
///   - Tet (K=3): C_3 = sqrt(6), range [1, inf), ideal = 1.
///
/// Four-layer design:
///   - **Layer 1 (zipper-concepts core)**: constrained on
///   `::zipper::concepts::Vector`.
///   - **Layer 1.5 (PointSpan wrappers)**: thin overloads converting spans.
///   - **Layer 1.5M (Matrix wrappers)**: overloads accepting a D×N matrix.
///   - **Layer 1.5ID (HasAsTuple wrappers)**: for IncidentData and similar.

#include <balsa/geometry/simplex/concepts.hpp>
#include <balsa/geometry/simplex/edge_length.hpp>
#include <balsa/geometry/simplex/inradius.hpp>

#include <cmath>
#include <cstddef>
#include <limits>
#include <span>
#include <tuple>
#include <utility>

#include <zipper/VectorBase.hpp>
#include <zipper/concepts/Matrix.hpp>
#include <zipper/concepts/Vector.hpp>

namespace balsa::geometry::simplex {

namespace detail {

    /// Normalization constant C_k = sqrt(k*(k+1)/2) for aspect ratio.
    template <typename T, std::size_t K>
    constexpr auto aspect_ratio_normalization() -> T {
        return std::sqrt(T(K) * T(K + 1) / T{2});
    }

} // namespace detail

// ============================================================================
// Layer 1: zipper-concepts core
// ============================================================================

/// Aspect ratio of a K-simplex: l_max / (2 * C_k * r) (zipper core).
///
/// Range [1, inf), ideal = 1 for regular simplices.
/// Returns infinity for degenerate simplices.
template <::zipper::concepts::Vector V, ::zipper::concepts::Vector... Rest>
    requires(sizeof...(Rest) >= 2) && (std::same_as<V, Rest> && ...)
auto aspect_ratio(const V &v0, const Rest &...rest) -> typename V::value_type {
    using T = typename V::value_type;
    constexpr auto K = sizeof...(Rest);

    T l_max = max_edge_length(v0, rest...);
    T r = inradius(v0, rest...);

    if (r <= T{0}) return std::numeric_limits<T>::infinity();

    constexpr T C_k = detail::aspect_ratio_normalization<T, K>();
    return l_max / (T{2} * C_k * r);
}

// ============================================================================
// Layer 1.5: PointSpan wrappers
// ============================================================================

/// Aspect ratio of a K-simplex (PointSpan wrapper).
template <PointSpan S, PointSpan... Rest>
    requires(sizeof...(Rest) >= 2) && (std::same_as<S, Rest> && ...)
auto aspect_ratio(S v0, Rest... rest) -> scalar_t<S> {
    return aspect_ratio(::zipper::VectorBase(v0),
                        ::zipper::VectorBase(rest)...);
}

// ============================================================================
// Layer 1.5M: Matrix wrappers
// ============================================================================

/// Aspect ratio from a D×N matrix of vertex columns (N >= 3).
template <::zipper::concepts::Matrix M>
    requires(std::decay_t<M>::extents_type::static_extent(1)
             != std::dynamic_extent)
            && (std::decay_t<M>::extents_type::static_extent(1) >= 3)
auto aspect_ratio(const M &pts) -> typename std::decay_t<M>::value_type {
    using T = typename std::decay_t<M>::value_type;
    constexpr auto N = std::decay_t<M>::extents_type::static_extent(1);
    constexpr auto K = N - 1;

    T l_max = max_edge_length(pts);
    T r = inradius(pts);

    if (r <= T{0}) return std::numeric_limits<T>::infinity();

    constexpr T C_k = detail::aspect_ratio_normalization<T, K>();
    return l_max / (T{2} * C_k * r);
}

// ============================================================================
// Layer 1.5ID: HasAsTuple wrappers (IncidentData and similar)
// ============================================================================

/// Aspect ratio of a simplex (HasAsTuple wrapper).
template <HasAsTuple ID>
auto aspect_ratio(const ID &id) {
    return std::apply(
        [](const auto &...vs) {
            return aspect_ratio(::zipper::VectorBase(vs)...);
        },
        id.as_tuple());
}

} // namespace balsa::geometry::simplex
#endif
