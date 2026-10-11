#if !defined(BALSA_GEOMETRY_SIMPLEX_CLOSEST_POINT_HPP)
#define BALSA_GEOMETRY_SIMPLEX_CLOSEST_POINT_HPP

/// @file closest_point.hpp
/// @brief Closest point on a simplex and point-to-simplex distance.
///
/// Generic dimension-agnostic closest-point computation for simplices:
///   - `closest_point_on_simplex<V, N>(query, vertices)` returns the
///     closest point, barycentric coordinates, and squared distance.
///   - `distance_sq_to_simplex<V, N>(query, vertices)` returns just the
///     squared distance.
///
/// Specialized fast paths for N=2 (edge) and N=3 (triangle).
/// General N>=4 uses recursive boundary descent.
///
/// Four-layer design:
///   - **Layer 1 (zipper-concepts core)**: constrained on
///   `::zipper::concepts::Vector`.
///   - **Layer 1.5 (PointSpan wrappers)**: thin overloads converting
///     `std::span<const T, N>` to zipper via `VectorBase(span)` CTAD.
///   - **Layer 1.5ID (HasAsTuple wrappers)**: for IncidentData and similar.

#include <balsa/geometry/simplex/concepts.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <numeric>
#include <span>
#include <tuple>
#include <utility>

#include <zipper/Vector.hpp>
#include <zipper/VectorBase.hpp>
#include <zipper/concepts/Vector.hpp>

namespace balsa::geometry::simplex {

// ============================================================================
// Result type
// ============================================================================

/// Result of closest-point-on-simplex computation.
///
/// @tparam SD  Spatial dimension (embedding dimension).
/// @tparam N   Number of simplex vertices (N=2 for edge, N=3 for triangle).
template <std::size_t SD, std::size_t N>
struct ClosestPointResult {
    /// The closest point on the simplex surface.
    std::array<double, SD> point;

    /// Barycentric coordinates (all N values, sum to 1).
    /// `closest = sum_i(barycentric[i] * vertex[i])`.
    std::array<double, N> barycentric;

    /// Squared Euclidean distance from the query point to the closest point.
    double distance_sq;
};

// ============================================================================
// Layer 1: zipper-concepts core
// ============================================================================

namespace detail {

    /// Closest point on an edge [a, b] to query point p.
    ///
    /// Projects p onto the line through a and b, clamps the parameter
    /// to [0, 1], and returns the clamped point with barycentric coords.
    template <::zipper::concepts::Vector V>
    auto closest_point_edge_impl(const V &p, const V &a, const V &b)
        -> ClosestPointResult<V::extents_type::static_extent(0), 2> {
        constexpr auto SD = V::extents_type::static_extent(0);
        using T = typename V::value_type;

        auto ab = (b - a).eval();
        auto ap = (p - a).eval();
        T d = ab.dot(ab);

        // Degenerate edge (a == b).
        T t;
        if (d <= T{0}) {
            t = T{0};
        } else {
            t = std::clamp(ap.dot(ab) / d, T{0}, T{1});
        }

        auto closest = (a + t * ab).eval();
        auto diff = (p - closest).eval();

        ClosestPointResult<SD, 2> result;
        result.barycentric = {T{1} - t, t};
        result.distance_sq = static_cast<double>(diff.dot(diff));
        for (std::size_t i = 0; i < SD; ++i) {
            result.point[i] = static_cast<double>(closest(i));
        }
        return result;
    }

    /// Closest point on a triangle [a, b, c] to query point p.
    ///
    /// Uses the Voronoi region algorithm (Ericson, "Real-Time Collision
    /// Detection", Chapter 5.1.5).  Handles all degenerate cases
    /// (collinear vertices, coincident vertices).
    template <::zipper::concepts::Vector V>
    auto closest_point_triangle_impl(const V &p,
                                     const V &a,
                                     const V &b,
                                     const V &c)
        -> ClosestPointResult<V::extents_type::static_extent(0), 3> {
        constexpr auto SD = V::extents_type::static_extent(0);
        using T = typename V::value_type;

        auto ab = (b - a).eval();
        auto ac = (c - a).eval();
        auto ap = (p - a).eval();

        T d1 = ab.dot(ap);
        T d2 = ac.dot(ap);

        // Check if P is in vertex region outside A.
        if (d1 <= T{0} && d2 <= T{0}) {
            auto diff = (p - a).eval();
            ClosestPointResult<SD, 3> result;
            result.barycentric = {T{1}, T{0}, T{0}};
            result.distance_sq = static_cast<double>(diff.dot(diff));
            for (std::size_t i = 0; i < SD; ++i)
                result.point[i] = static_cast<double>(a(i));
            return result;
        }

        auto bp = (p - b).eval();
        T d3 = ab.dot(bp);
        T d4 = ac.dot(bp);

        // Check if P is in vertex region outside B.
        if (d3 >= T{0} && d4 <= d3) {
            auto diff = (p - b).eval();
            ClosestPointResult<SD, 3> result;
            result.barycentric = {T{0}, T{1}, T{0}};
            result.distance_sq = static_cast<double>(diff.dot(diff));
            for (std::size_t i = 0; i < SD; ++i)
                result.point[i] = static_cast<double>(b(i));
            return result;
        }

        // Check if P is in edge region of AB.
        T vc = d1 * d4 - d3 * d2;
        if (vc <= T{0} && d1 >= T{0} && d3 <= T{0}) {
            T v = d1 / (d1 - d3);
            auto closest = (a + v * ab).eval();
            auto diff = (p - closest).eval();
            ClosestPointResult<SD, 3> result;
            result.barycentric = {T{1} - v, v, T{0}};
            result.distance_sq = static_cast<double>(diff.dot(diff));
            for (std::size_t i = 0; i < SD; ++i)
                result.point[i] = static_cast<double>(closest(i));
            return result;
        }

        auto cp = (p - c).eval();
        T d5 = ab.dot(cp);
        T d6 = ac.dot(cp);

        // Check if P is in vertex region outside C.
        if (d6 >= T{0} && d5 <= d6) {
            auto diff = (p - c).eval();
            ClosestPointResult<SD, 3> result;
            result.barycentric = {T{0}, T{0}, T{1}};
            result.distance_sq = static_cast<double>(diff.dot(diff));
            for (std::size_t i = 0; i < SD; ++i)
                result.point[i] = static_cast<double>(c(i));
            return result;
        }

        // Check if P is in edge region of AC.
        T vb = d5 * d2 - d1 * d6;
        if (vb <= T{0} && d2 >= T{0} && d6 <= T{0}) {
            T w = d2 / (d2 - d6);
            auto closest = (a + w * ac).eval();
            auto diff = (p - closest).eval();
            ClosestPointResult<SD, 3> result;
            result.barycentric = {T{1} - w, T{0}, w};
            result.distance_sq = static_cast<double>(diff.dot(diff));
            for (std::size_t i = 0; i < SD; ++i)
                result.point[i] = static_cast<double>(closest(i));
            return result;
        }

        // Check if P is in edge region of BC.
        T va = d3 * d6 - d5 * d4;
        if (va <= T{0} && (d4 - d3) >= T{0} && (d5 - d6) >= T{0}) {
            T w = (d4 - d3) / ((d4 - d3) + (d5 - d6));
            auto closest = (b + w * (c - b)).eval();
            auto diff = (p - closest).eval();
            ClosestPointResult<SD, 3> result;
            result.barycentric = {T{0}, T{1} - w, w};
            result.distance_sq = static_cast<double>(diff.dot(diff));
            for (std::size_t i = 0; i < SD; ++i)
                result.point[i] = static_cast<double>(closest(i));
            return result;
        }

        // P is inside the triangle.
        T denom = T{1} / (va + vb + vc);
        T v = vb * denom;
        T w = vc * denom;
        auto closest = (a + v * ab + w * ac).eval();
        auto diff = (p - closest).eval();
        ClosestPointResult<SD, 3> result;
        result.barycentric = {T{1} - v - w, v, w};
        result.distance_sq = static_cast<double>(diff.dot(diff));
        for (std::size_t i = 0; i < SD; ++i)
            result.point[i] = static_cast<double>(closest(i));
        return result;
    }

    /// General closest-point-on-simplex via recursive boundary descent.
    ///
    /// For an (N-1)-simplex with N vertices, projects the query onto
    /// the affine hull.  If the projection lies inside (all barycentric
    /// coords non-negative), that is the closest point.  Otherwise,
    /// recursively finds the closest point on each boundary (N-2)-face
    /// and returns the best.
    template <::zipper::concepts::Vector V, std::size_t N>
        requires(N >= 4)
    auto closest_point_general_impl(
        const V &p,
        const std::array<::zipper::Vector<typename V::value_type,
                                          V::extents_type::static_extent(0)>,
                         N> &verts)
        -> ClosestPointResult<V::extents_type::static_extent(0), N> {
        constexpr auto SD = V::extents_type::static_extent(0);
        using T = typename V::value_type;
        using Vec = ::zipper::Vector<T, SD>;

        // Best result found among all boundary faces.
        ClosestPointResult<SD, N> best;
        best.distance_sq = std::numeric_limits<double>::infinity();

        // Try each boundary face (omit one vertex at a time).
        [&]<std::size_t... Faces>(std::index_sequence<Faces...>) {
            (([&] {
                 constexpr std::size_t skip = Faces;

                 // Build the sub-simplex vertices (N-1 vertices, omitting index
                 // skip).
                 std::array<Vec, N - 1> sub_verts;
                 std::size_t idx = 0;
                 for (std::size_t i = 0; i < N; ++i) {
                     if (i != skip) { sub_verts[idx++] = verts[i]; }
                 }

                 // Recursively find closest point on this face.
                 auto sub = closest_point_on_simplex(p, sub_verts);

                 if (sub.distance_sq < best.distance_sq) {
                     best.distance_sq = sub.distance_sq;
                     best.point = sub.point;
                     // Map sub-barycentric back to full barycentric.
                     best.barycentric = {};
                     idx = 0;
                     for (std::size_t i = 0; i < N; ++i) {
                         if (i != skip) {
                             best.barycentric[i] = sub.barycentric[idx++];
                         }
                     }
                 }
             }()),
             ...);
        }(std::make_index_sequence<N>{});

        return best;
    }

} // namespace detail

/// Closest point on an (N-1)-simplex with N vertices to a query point.
///
/// @tparam V  A zipper vector type.
/// @tparam N  Number of simplex vertices (2 = edge, 3 = triangle, ...).
///
/// @param p     The query point.
/// @param verts The N simplex vertices.
/// @return A `ClosestPointResult<SD, N>` with the closest point,
///         barycentric coordinates, and squared distance.
template <::zipper::concepts::Vector V, std::size_t N>
    requires(N >= 2)
auto closest_point_on_simplex(
    const V &p,
    const std::array<::zipper::Vector<typename V::value_type,
                                      V::extents_type::static_extent(0)>,
                     N> &verts)
    -> ClosestPointResult<V::extents_type::static_extent(0), N> {
    if constexpr (N == 2) {
        return detail::closest_point_edge_impl(p, verts[0], verts[1]);
    } else if constexpr (N == 3) {
        return detail::closest_point_triangle_impl(
            p, verts[0], verts[1], verts[2]);
    } else {
        return detail::closest_point_general_impl<V, N>(p, verts);
    }
}

/// Squared distance from a query point to an (N-1)-simplex.
///
/// Convenience wrapper around `closest_point_on_simplex`.
template <::zipper::concepts::Vector V, std::size_t N>
    requires(N >= 2)
auto distance_sq_to_simplex(
    const V &p,
    const std::array<::zipper::Vector<typename V::value_type,
                                      V::extents_type::static_extent(0)>,
                     N> &verts) -> double {
    return closest_point_on_simplex(p, verts).distance_sq;
}

// ============================================================================
// Layer 1.5: PointSpan wrappers
// ============================================================================

/// Closest point on an (N-1)-simplex (PointSpan wrapper).
///
/// Accepts `std::span<const T, SD>` arguments and converts to zipper vectors.
template <PointSpan S, std::size_t N>
    requires(N >= 2)
auto closest_point_on_simplex(
    S p,
    const std::array<std::array<scalar_t<S>, dim_v<S>>, N> &verts)
    -> ClosestPointResult<dim_v<S>, N> {
    using T = scalar_t<S>;
    constexpr auto SD = dim_v<S>;
    using Vec = ::zipper::Vector<T, SD>;

    auto pv = Vec(::zipper::VectorBase(p));
    std::array<Vec, N> zverts;
    for (std::size_t i = 0; i < N; ++i) {
        zverts[i] = Vec(::zipper::VectorBase(std::span<const T, SD>(verts[i])));
    }
    return closest_point_on_simplex(pv, zverts);
}

/// Squared distance from a point to an (N-1)-simplex (PointSpan wrapper).
template <PointSpan S, std::size_t N>
    requires(N >= 2)
auto distance_sq_to_simplex(
    S p,
    const std::array<std::array<scalar_t<S>, dim_v<S>>, N> &verts) -> double {
    return closest_point_on_simplex(p, verts).distance_sq;
}

// ============================================================================
// Layer 1.5ID: HasAsTuple wrappers (IncidentData and similar)
// ============================================================================

/// Closest point on a simplex (HasAsTuple wrapper).
///
/// The first element of the tuple is the query point, the remaining
/// elements are the simplex vertices.
///
/// Returns a `ClosestPointResult` with `std::array` types.
template <HasAsTuple ID>
auto closest_point_on_simplex(const ID &id) {
    return std::apply(
        [](const auto &p, const auto &...vs) {
            using T = std::remove_const_t<
                typename std::decay_t<decltype(p)>::value_type>;
            constexpr auto SD = std::decay_t<decltype(p)>::extent;
            constexpr auto N = sizeof...(vs);
            using Vec = ::zipper::Vector<T, SD>;

            auto pv = Vec(::zipper::VectorBase(p));
            std::array<Vec, N> verts = {Vec(::zipper::VectorBase(vs))...};
            return closest_point_on_simplex(pv, verts);
        },
        id.as_tuple());
}

/// Squared distance from a point to a simplex (HasAsTuple wrapper).
template <HasAsTuple ID>
auto distance_sq_to_simplex(const ID &id) {
    return closest_point_on_simplex(id).distance_sq;
}

} // namespace balsa::geometry::simplex
#endif
