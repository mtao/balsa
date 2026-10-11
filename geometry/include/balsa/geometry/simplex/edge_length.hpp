#if !defined(BALSA_GEOMETRY_SIMPLEX_EDGE_LENGTH_HPP)
#define BALSA_GEOMETRY_SIMPLEX_EDGE_LENGTH_HPP

/// @file edge_length.hpp
/// @brief Edge length and maximum edge length for simplices.
///
/// `edge_length(a, b)` computes the Euclidean distance between two points.
/// This is equivalent to `volume_unsigned(a, b)` (unsigned volume of a
/// 1-simplex = length).
///
/// `max_edge_length(v0, v1, ...)` computes the maximum edge length over
/// all vertex pairs of a simplex. For a K-simplex with K+1 vertices,
/// this iterates over all K*(K+1)/2 edges.
///
/// Four-layer design:
///   - **Layer 1 (zipper-concepts core)**: constrained on
///   `::zipper::concepts::Vector`.
///   - **Layer 1.5 (PointSpan wrappers)**: thin overloads converting spans.
///   - **Layer 1.5M (Matrix wrappers)**: overloads accepting a D×N matrix.
///   - **Layer 1.5ID (HasAsTuple wrappers)**: for IncidentData and similar.

#include <balsa/geometry/simplex/concepts.hpp>
#include <balsa/geometry/simplex/volume.hpp>

#include <algorithm>
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

/// Edge length (Euclidean distance) between two points (zipper core).
///
/// Equivalent to `volume_unsigned(a, b)` — the unsigned volume of a 1-simplex.
template <::zipper::concepts::Vector V>
auto edge_length(const V &a, const V &b) -> typename V::value_type {
    return volume_unsigned(a, b);
}

namespace detail {

    /// Helper: compute the norm of (vi - vj) for zipper vectors stored in a
    /// tuple.
    template <typename Tuple, std::size_t I, std::size_t J>
    auto edge_length_pair(const Tuple &verts) {
        auto diff = (std::get<I>(verts) - std::get<J>(verts)).eval();
        return diff.norm();
    }

    /// Helper: max edge length over all pairs via nested index sequences.
    /// Iterates over all pairs (i, j) with i < j.
    template <typename Tuple, std::size_t I, std::size_t... Js>
    auto max_edge_from_i(const Tuple &verts, std::index_sequence<Js...>) {
        using T = decltype(edge_length_pair<Tuple, I, I + 1>(verts));
        return std::max<T>({edge_length_pair<Tuple, I, I + 1 + Js>(verts)...});
    }

    template <typename Tuple, std::size_t... Is>
    auto max_edge_all(const Tuple &verts, std::index_sequence<Is...>) {
        constexpr auto N = std::tuple_size_v<Tuple>;
        // For each vertex I, iterate over all J > I
        return std::max({max_edge_from_i<Tuple, Is>(
            verts, std::make_index_sequence<N - Is - 1>{})...});
    }

} // namespace detail

/// Maximum edge length of a simplex (zipper core).
///
/// Iterates over all K*(K+1)/2 vertex pairs and returns the maximum distance.
template <::zipper::concepts::Vector V, ::zipper::concepts::Vector... Rest>
    requires(sizeof...(Rest) >= 1) && (std::same_as<V, Rest> && ...)
auto max_edge_length(const V &v0, const Rest &...rest) ->
    typename V::value_type {
    auto verts = std::tie(v0, rest...);
    constexpr auto N = 1 + sizeof...(Rest);
    return detail::max_edge_all(verts, std::make_index_sequence<N - 1>{});
}

// ============================================================================
// Layer 1.5: PointSpan wrappers
// ============================================================================

/// Edge length between two points (PointSpan wrapper).
template <PointSpan S>
auto edge_length(S a, S b) -> scalar_t<S> {
    return edge_length(::zipper::VectorBase(a), ::zipper::VectorBase(b));
}

/// Maximum edge length of a simplex (PointSpan wrapper).
template <PointSpan S, PointSpan... Rest>
    requires(sizeof...(Rest) >= 1) && (std::same_as<S, Rest> && ...)
auto max_edge_length(S v0, Rest... rest) -> scalar_t<S> {
    return max_edge_length(::zipper::VectorBase(v0),
                           ::zipper::VectorBase(rest)...);
}

// ============================================================================
// Layer 1.5M: Matrix wrappers
// ============================================================================

/// Maximum edge length from a D×N matrix of vertex columns.
template <::zipper::concepts::Matrix M>
    requires(std::decay_t<M>::extents_type::static_extent(1)
             != std::dynamic_extent)
            && (std::decay_t<M>::extents_type::static_extent(1) >= 2)
auto max_edge_length(const M &pts) -> typename std::decay_t<M>::value_type {
    using T = typename std::decay_t<M>::value_type;
    constexpr auto N = std::decay_t<M>::extents_type::static_extent(1);

    // Iterate over all pairs (i, j) with i < j
    T result = T{0};
    [&]<std::size_t... Is>(std::index_sequence<Is...>) {
        (([&]<std::size_t I>(std::integral_constant<std::size_t, I>) {
             [&]<std::size_t... Js>(std::index_sequence<Js...>) {
                 ((result = std::max(
                       result,
                       (pts.col(I) - pts.col(I + 1 + Js)).eval().norm())),
                  ...);
             }(std::make_index_sequence<N - I - 1>{});
         }(std::integral_constant<std::size_t, Is>{})),
         ...);
    }(std::make_index_sequence<N - 1>{});

    return result;
}

// ============================================================================
// Layer 1.5ID: HasAsTuple wrappers (IncidentData and similar)
// ============================================================================

/// Edge length between two points (HasAsTuple wrapper).
///
/// The tuple must contain exactly 2 elements.
template <HasAsTuple ID>
auto edge_length(const ID &id) {
    return std::apply(
        [](const auto &a, const auto &b) {
            return edge_length(::zipper::VectorBase(a),
                               ::zipper::VectorBase(b));
        },
        id.as_tuple());
}

/// Maximum edge length of a simplex (HasAsTuple wrapper).
template <HasAsTuple ID>
auto max_edge_length(const ID &id) {
    return std::apply(
        [](const auto &...vs) {
            return max_edge_length(::zipper::VectorBase(vs)...);
        },
        id.as_tuple());
}

} // namespace balsa::geometry::simplex
#endif
