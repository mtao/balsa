#if !defined(BALSA_GEOMETRY_SIMPLEX_BOUNDARY_VOLUME_HPP)
#define BALSA_GEOMETRY_SIMPLEX_BOUNDARY_VOLUME_HPP

/// @file boundary_volume.hpp
/// @brief Boundary volume of simplices (perimeter for triangles, surface area
/// for tetrahedra).
///
/// `boundary_volume(v0, v1, ...)` computes the sum of unsigned volumes of
/// all (K-1)-dimensional faces of a K-simplex. Each face is formed by
/// omitting one vertex.
///
/// - Triangle (K=2): sum of 3 edge lengths = perimeter
/// - Tetrahedron (K=3): sum of 4 triangle areas = surface area
///
/// Requires at least 3 vertices (K >= 2).
///
/// Four-layer design:
///   - **Layer 1 (zipper-concepts core)**: constrained on
///   `::zipper::concepts::Vector`.
///   - **Layer 1.5 (PointSpan wrappers)**: thin overloads converting spans.
///   - **Layer 1.5M (Matrix wrappers)**: overloads accepting a D×N matrix.
///   - **Layer 1.5ID (HasAsTuple wrappers)**: for IncidentData and similar.

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

namespace detail {

    /// Compute volume_unsigned of the face formed by omitting vertex `Skip`
    /// from the tuple of vertices.
    ///
    /// For a tuple (v0, v1, ..., vN-1), omitting vertex Skip yields:
    ///   volume_unsigned(v0, ..., v_{Skip-1}, v_{Skip+1}, ..., v_{N-1})
    template <std::size_t Skip,
              typename Tuple,
              std::size_t... Pre,
              std::size_t... Post>
    auto volume_excluding_vertex(const Tuple &verts,
                                 std::index_sequence<Pre...>,
                                 std::index_sequence<Post...>) {
        return volume_unsigned(std::get<Pre>(verts)...,
                               std::get<Skip + 1 + Post>(verts)...);
    }

    /// Sum of face volumes, iterating over all faces via index sequence.
    template <typename Tuple, std::size_t... Faces>
    auto boundary_volume_impl(const Tuple &verts,
                              std::index_sequence<Faces...>) {
        constexpr auto N = std::tuple_size_v<Tuple>;
        return (volume_excluding_vertex<Faces>(
                    verts,
                    std::make_index_sequence<Faces>{},
                    std::make_index_sequence<N - Faces - 1>{})
                + ...);
    }

} // namespace detail

// ============================================================================
// Layer 1: zipper-concepts core
// ============================================================================

/// Boundary volume of a K-simplex (zipper core).
///
/// Sum of unsigned volumes of all (K-1)-faces.
/// Requires at least 3 vertices (K >= 2).
template <::zipper::concepts::Vector V, ::zipper::concepts::Vector... Rest>
    requires(sizeof...(Rest) >= 2) && (std::same_as<V, Rest> && ...)
auto boundary_volume(const V &v0, const Rest &...rest) ->
    typename V::value_type {
    constexpr auto N = 1 + sizeof...(Rest);
    auto verts = std::tie(v0, rest...);
    return detail::boundary_volume_impl(verts, std::make_index_sequence<N>{});
}

// ============================================================================
// Layer 1.5: PointSpan wrappers
// ============================================================================

/// Boundary volume of a K-simplex (PointSpan wrapper).
template <PointSpan S, PointSpan... Rest>
    requires(sizeof...(Rest) >= 2) && (std::same_as<S, Rest> && ...)
auto boundary_volume(S v0, Rest... rest) -> scalar_t<S> {
    return boundary_volume(::zipper::VectorBase(v0),
                           ::zipper::VectorBase(rest)...);
}

// ============================================================================
// Layer 1.5M: Matrix wrappers
// ============================================================================

/// Boundary volume from a D×N matrix of vertex columns (N >= 3).
template <::zipper::concepts::Matrix M>
    requires(std::decay_t<M>::extents_type::static_extent(1)
             != std::dynamic_extent)
            && (std::decay_t<M>::extents_type::static_extent(1) >= 3)
auto boundary_volume(const M &pts) -> typename std::decay_t<M>::value_type {
    using T = typename std::decay_t<M>::value_type;
    constexpr auto N = std::decay_t<M>::extents_type::static_extent(1);

    // For each face (omit column i), compute volume_unsigned of remaining
    // columns
    T result = T{0};
    [&]<std::size_t... Is>(std::index_sequence<Is...>) {
        (([&]<std::size_t Skip>(std::integral_constant<std::size_t, Skip>) {
             // Build a D×(N-1) sub-matrix omitting column Skip
             constexpr auto D = std::decay_t<M>::extents_type::static_extent(0);
             ::zipper::Matrix<T, D, N - 1> sub;
             std::size_t col = 0;
             for (std::size_t j = 0; j < N; ++j) {
                 if (j == Skip) continue;
                 sub.col(col) = pts.col(j);
                 ++col;
             }
             result += volume_unsigned(sub);
         }(std::integral_constant<std::size_t, Is>{})),
         ...);
    }(std::make_index_sequence<N>{});

    return result;
}

// ============================================================================
// Layer 1.5ID: HasAsTuple wrappers (IncidentData and similar)
// ============================================================================

/// Boundary volume of a simplex (HasAsTuple wrapper).
template <HasAsTuple ID>
auto boundary_volume(const ID &id) {
    return std::apply(
        [](const auto &...vs) {
            return boundary_volume(::zipper::VectorBase(vs)...);
        },
        id.as_tuple());
}

} // namespace balsa::geometry::simplex
#endif
