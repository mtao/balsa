#if !defined(BALSA_GEOMETRY_SIMPLEX_SCALED_JACOBIAN_HPP)
#define BALSA_GEOMETRY_SIMPLEX_SCALED_JACOBIAN_HPP

/// @file scaled_jacobian.hpp
/// @brief Scaled Jacobian quality metric for simplices.
///
/// Generic `scaled_jacobian(v0, v1, ...)` dispatches based on vertex count:
///
/// Triangle (K=2): det(J) / (||c1|| * ||c2||) where J maps the reference
///   equilateral to the physical triangle. Uses the jacobian primitive.
///   Range [-1, 1], ideal = 1.
///
/// Tet (K=3): **Special case** — per-corner evaluation using raw edge
///   matrices (NOT a reference element). Evaluates at all 4 corners using
///   even permutations, takes the minimum.
///   Per-corner: sqrt(2) * det(E) / (||e1|| * ||e2|| * ||e3||)
///   Range [-1, 1], ideal = 1.
///
/// Four-layer design:
///   - **Layer 1 (zipper-concepts core)**: constrained on
///   `::zipper::concepts::Vector`.
///   - **Layer 1.5 (PointSpan wrappers)**: thin overloads converting spans.
///   - **Layer 1.5M (Matrix wrappers)**: overloads accepting a D×N matrix.
///   - **Layer 1.5ID (HasAsTuple wrappers)**: for IncidentData and similar.

#include <balsa/geometry/simplex/concepts.hpp>
#include <balsa/geometry/simplex/edge_matrix.hpp>
#include <balsa/geometry/simplex/jacobian.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <numbers>
#include <span>
#include <tuple>
#include <utility>

#include <zipper/Matrix.hpp>
#include <zipper/Vector.hpp>
#include <zipper/VectorBase.hpp>
#include <zipper/concepts/Matrix.hpp>
#include <zipper/concepts/Vector.hpp>
#include <zipper/utils/determinant.hpp>

namespace balsa::geometry::simplex {

namespace detail {

    /// Scaled Jacobian for a triangle via the Jacobian primitive.
    ///
    /// SJ = det(J) / (||j0|| * ||j1||)
    template <::zipper::concepts::Vector V>
    auto scaled_jacobian_tri_impl(const V &a, const V &b, const V &c) ->
        typename V::value_type {
        using T = typename V::value_type;
        constexpr auto N = V::extents_type::static_extent(0);

        auto J = jacobian(a, b, c);
        auto j0 = J.col(std::size_t{0});
        auto j1 = J.col(std::size_t{1});

        T norm0 = j0.norm();
        T norm1 = j1.norm();
        T denom = norm0 * norm1;
        if (denom <= T{0}) return T{0};

        T det_j;
        if constexpr (N == 2) {
            det_j = j0(0) * j1(1) - j0(1) * j1(0);
        } else if constexpr (N == 3) {
            ::zipper::Vector<T, N> cross = j0.cross(j1);
            det_j = cross.norm();
        } else {
            det_j = j0(0) * j1(1) - j0(1) * j1(0);
        }

        return std::clamp(det_j / denom, T{-1}, T{1});
    }

    /// Per-corner scaled Jacobian for a tet (Verdict/Knupp formulation).
    ///
    /// sqrt(2) * det([e1|e2|e3]) / (||e1|| * ||e2|| * ||e3||)
    template <::zipper::concepts::Vector V>
        requires(V::extents_type::static_extent(0) == 3)
    auto tet_corner_sj(const V &p0, const V &p1, const V &p2, const V &p3) ->
        typename V::value_type {
        using T = typename V::value_type;

        ::zipper::Vector<T, 3> e1 = p1 - p0;
        ::zipper::Vector<T, 3> e2 = p2 - p0;
        ::zipper::Vector<T, 3> e3 = p3 - p0;

        ::zipper::Matrix<T, 3, 3> E{{e1(0), e2(0), e3(0)},
                                    {e1(1), e2(1), e3(1)},
                                    {e1(2), e2(2), e3(2)}};

        T det = ::zipper::utils::determinant(E);
        T n1 = e1.norm();
        T n2 = e2.norm();
        T n3 = e3.norm();
        T denom = n1 * n2 * n3;
        if (denom <= T{0}) return T{0};

        return std::numbers::sqrt2_v<T> * det / denom;
    }

    /// Scaled Jacobian for a tet: min over 4 even-permutation corners.
    template <::zipper::concepts::Vector V>
        requires(V::extents_type::static_extent(0) == 3)
    auto
        scaled_jacobian_tet_impl(const V &a, const V &b, const V &c, const V &d)
            -> typename V::value_type {
        using T = typename V::value_type;

        T sj_a = tet_corner_sj(a, b, c, d);
        T sj_b = tet_corner_sj(b, a, d, c);
        T sj_c = tet_corner_sj(c, a, b, d);
        T sj_d = tet_corner_sj(d, a, c, b);

        T result = std::min({sj_a, sj_b, sj_c, sj_d});
        return std::clamp(result, T{-1}, T{1});
    }

} // namespace detail

// ============================================================================
// Layer 1: zipper-concepts core
// ============================================================================

/// Scaled Jacobian of a K-simplex (zipper core).
///
/// Dispatches to triangle or tet implementation based on vertex count.
/// Triangle uses Jacobian primitive; tet uses per-corner Verdict formulation.
template <::zipper::concepts::Vector V, ::zipper::concepts::Vector... Rest>
    requires(sizeof...(Rest) >= 2) && (std::same_as<V, Rest> && ...)
auto scaled_jacobian(const V &v0, const Rest &...rest) ->
    typename V::value_type {
    constexpr auto K = sizeof...(Rest);
    if constexpr (K == 2) {
        return [&](const auto &b, const auto &c) {
            return detail::scaled_jacobian_tri_impl(v0, b, c);
        }(rest...);
    } else if constexpr (K == 3) {
        return [&](const auto &b, const auto &c, const auto &d) {
            return detail::scaled_jacobian_tet_impl(v0, b, c, d);
        }(rest...);
    } else {
        static_assert(
            K == 2 || K == 3,
            "scaled_jacobian only supports triangles (K=2) and tets (K=3)");
    }
}

// ============================================================================
// Layer 1.5: PointSpan wrappers
// ============================================================================

/// Scaled Jacobian of a K-simplex (PointSpan wrapper).
template <PointSpan S, PointSpan... Rest>
    requires(sizeof...(Rest) >= 2) && (std::same_as<S, Rest> && ...)
auto scaled_jacobian(S v0, Rest... rest) -> scalar_t<S> {
    return scaled_jacobian(::zipper::VectorBase(v0),
                           ::zipper::VectorBase(rest)...);
}

// ============================================================================
// Layer 1.5M: Matrix wrappers
// ============================================================================

/// Scaled Jacobian from a D×N matrix of vertex columns (N=3 or N=4).
template <::zipper::concepts::Matrix M>
    requires(std::decay_t<M>::extents_type::static_extent(1)
             != std::dynamic_extent)
            && (std::decay_t<M>::extents_type::static_extent(1) >= 3)
            && (std::decay_t<M>::extents_type::static_extent(1) <= 4)
auto scaled_jacobian(const M &pts) -> typename std::decay_t<M>::value_type {
    constexpr auto N = std::decay_t<M>::extents_type::static_extent(1);
    if constexpr (N == 3) {
        return scaled_jacobian(pts.col(std::size_t{0}),
                               pts.col(std::size_t{1}),
                               pts.col(std::size_t{2}));
    } else {
        return scaled_jacobian(pts.col(std::size_t{0}),
                               pts.col(std::size_t{1}),
                               pts.col(std::size_t{2}),
                               pts.col(std::size_t{3}));
    }
}

// ============================================================================
// Layer 1.5ID: HasAsTuple wrappers (IncidentData and similar)
// ============================================================================

/// Scaled Jacobian of a simplex (HasAsTuple wrapper).
template <HasAsTuple ID>
auto scaled_jacobian(const ID &id) {
    return std::apply(
        [](const auto &...vs) {
            return scaled_jacobian(::zipper::VectorBase(vs)...);
        },
        id.as_tuple());
}

} // namespace balsa::geometry::simplex
#endif
