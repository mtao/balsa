#if !defined(BALSA_GEOMETRY_SIMPLEX_AMIPS_HPP)
#define BALSA_GEOMETRY_SIMPLEX_AMIPS_HPP

/// @file amips.hpp
/// @brief AMIPS (Advanced MIPS) energy quality metric for simplices.
///
/// Generic `amips(v0, v1, ...)` dispatches based on vertex count:
///
/// Triangle (K=2): E = tr(J^T J) / det(J), minimum = 2 at equilateral.
/// Tet (K=3): E = tr(J^T J) / det(J)^(2/3), minimum = 3 at regular.
///
/// Uses the Jacobian primitive with equilateral/regular reference element.
/// Returns infinity for degenerate/inverted elements (det(J) <= 0).
///
/// Four-layer design:
///   - **Layer 1 (zipper-concepts core)**: constrained on
///   `::zipper::concepts::Vector`.
///   - **Layer 1.5 (PointSpan wrappers)**: thin overloads converting spans.
///   - **Layer 1.5M (Matrix wrappers)**: overloads accepting a D×N matrix.
///   - **Layer 1.5ID (HasAsTuple wrappers)**: for IncidentData and similar.

#include <balsa/geometry/simplex/concepts.hpp>
#include <balsa/geometry/simplex/jacobian.hpp>

#include <cmath>
#include <cstddef>
#include <limits>
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

    /// AMIPS for a triangle via the Jacobian primitive.
    ///
    /// AMIPS = tr(J^T J) / det(J), minimum = 2.
    template <::zipper::concepts::Vector V>
    auto amips_tri_impl(const V &a, const V &b, const V &c) ->
        typename V::value_type {
        using T = typename V::value_type;
        constexpr auto N = V::extents_type::static_extent(0);

        auto J = jacobian(a, b, c);
        auto j0 = J.col(std::size_t{0});
        auto j1 = J.col(std::size_t{1});

        T trace_jtj =
            j0.template norm_powered<2>() + j1.template norm_powered<2>();

        T det_j;
        if constexpr (N == 2) {
            det_j = j0(0) * j1(1) - j0(1) * j1(0);
        } else if constexpr (N == 3) {
            ::zipper::Vector<T, 3> cr = j0.cross(j1);
            det_j = cr.norm();
        } else {
            det_j = j0(0) * j1(1) - j0(1) * j1(0);
        }

        if (det_j <= T{0}) return std::numeric_limits<T>::infinity();
        return trace_jtj / det_j;
    }

    /// AMIPS for a tet via the Jacobian primitive.
    ///
    /// AMIPS = tr(J^T J) / det(J)^(2/3), minimum = 3.
    template <::zipper::concepts::Vector V>
        requires(V::extents_type::static_extent(0) == 3)
    auto amips_tet_impl(const V &a, const V &b, const V &c, const V &d) ->
        typename V::value_type {
        using T = typename V::value_type;

        auto J = jacobian(a, b, c, d);
        auto j0 = J.col(std::size_t{0});
        auto j1 = J.col(std::size_t{1});
        auto j2 = J.col(std::size_t{2});

        // det(J)
        ::zipper::Matrix<T, 3, 3> Jmat{{j0(0), j1(0), j2(0)},
                                       {j0(1), j1(1), j2(1)},
                                       {j0(2), j1(2), j2(2)}};
        T det_j = ::zipper::utils::determinant(Jmat);
        if (det_j <= T{0}) return std::numeric_limits<T>::infinity();

        T trace_jtj = j0.template norm_powered<2>()
                      + j1.template norm_powered<2>()
                      + j2.template norm_powered<2>();

        return trace_jtj / std::pow(det_j, T{2} / T{3});
    }

} // namespace detail

// ============================================================================
// Layer 1: zipper-concepts core
// ============================================================================

/// AMIPS energy of a K-simplex (zipper core).
///
/// Dispatches to triangle or tet implementation based on vertex count.
template <::zipper::concepts::Vector V, ::zipper::concepts::Vector... Rest>
    requires(sizeof...(Rest) >= 2) && (std::same_as<V, Rest> && ...)
auto amips(const V &v0, const Rest &...rest) -> typename V::value_type {
    constexpr auto K = sizeof...(Rest);
    if constexpr (K == 2) {
        return [&](const auto &b, const auto &c) {
            return detail::amips_tri_impl(v0, b, c);
        }(rest...);
    } else if constexpr (K == 3) {
        return [&](const auto &b, const auto &c, const auto &d) {
            return detail::amips_tet_impl(v0, b, c, d);
        }(rest...);
    } else {
        static_assert(K == 2 || K == 3,
                      "amips only supports triangles (K=2) and tets (K=3)");
    }
}

// ============================================================================
// Layer 1.5: PointSpan wrappers
// ============================================================================

/// AMIPS energy of a K-simplex (PointSpan wrapper).
template <PointSpan S, PointSpan... Rest>
    requires(sizeof...(Rest) >= 2) && (std::same_as<S, Rest> && ...)
auto amips(S v0, Rest... rest) -> scalar_t<S> {
    return amips(::zipper::VectorBase(v0), ::zipper::VectorBase(rest)...);
}

// ============================================================================
// Layer 1.5M: Matrix wrappers
// ============================================================================

/// AMIPS energy from a D×N matrix of vertex columns (N=3 or N=4).
template <::zipper::concepts::Matrix M>
    requires(std::decay_t<M>::extents_type::static_extent(1)
             != std::dynamic_extent)
            && (std::decay_t<M>::extents_type::static_extent(1) >= 3)
            && (std::decay_t<M>::extents_type::static_extent(1) <= 4)
auto amips(const M &pts) -> typename std::decay_t<M>::value_type {
    constexpr auto N = std::decay_t<M>::extents_type::static_extent(1);
    if constexpr (N == 3) {
        return amips(pts.col(std::size_t{0}),
                     pts.col(std::size_t{1}),
                     pts.col(std::size_t{2}));
    } else {
        return amips(pts.col(std::size_t{0}),
                     pts.col(std::size_t{1}),
                     pts.col(std::size_t{2}),
                     pts.col(std::size_t{3}));
    }
}

// ============================================================================
// Layer 1.5ID: HasAsTuple wrappers (IncidentData and similar)
// ============================================================================

/// AMIPS energy of a simplex (HasAsTuple wrapper).
template <HasAsTuple ID>
auto amips(const ID &id) {
    return std::apply(
        [](const auto &...vs) { return amips(::zipper::VectorBase(vs)...); },
        id.as_tuple());
}

} // namespace balsa::geometry::simplex
#endif
