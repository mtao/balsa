#if !defined(BALSA_GEOMETRY_SIMPLEX_POINT_IN_CIRCUMCIRCLE_HPP)
#define BALSA_GEOMETRY_SIMPLEX_POINT_IN_CIRCUMCIRCLE_HPP

/// @file point_in_circumcircle.hpp
/// @brief Test whether a point lies inside the circumsphere of a simplex.
///
/// Three-layer design:
///   - **Layer 1 (zipper-concepts core)**: vertex-vector + point vector API.
///   - **Layer 1.5 (PointSpan wrapper)**: thin overload converting spans.
///   - **Layer 1.5M (Matrix wrapper)**: accepts D×N matrix of vertex columns.

#include <balsa/geometry/simplex/circumcenter.hpp>
#include <balsa/geometry/simplex/concepts.hpp>

#include <cstddef>
#include <span>
#include <utility>

#include <zipper/Vector.hpp>
#include <zipper/VectorBase.hpp>
#include <zipper/concepts/Matrix.hpp>
#include <zipper/concepts/Vector.hpp>

#include "balsa/eigen/concepts/matrix_types.hpp"
#include "balsa/eigen/zipper_compat.hpp"

namespace balsa::geometry::simplex {

// ============================================================================
// Layer 1: zipper-concepts core
// ============================================================================

/// Returns true if point P lies strictly inside the circumsphere of the
/// simplex defined by vertices (v0, rest...).
template <::zipper::concepts::Vector V, ::zipper::concepts::Vector... Rest>
    requires(std::same_as<V, Rest> && ...)
auto point_in_circumsphere(const V &P, const V &v0, const Rest &...rest)
    -> bool {
    auto [C, r2] = circumcenter_with_squared_radius(v0, rest...);
    using T = typename V::value_type;
    constexpr auto D = V::extents_type::static_extent(0);
    ::zipper::Vector<T, D> diff = C - P;
    return diff.template norm_powered<2>() < r2;
}

// ============================================================================
// Layer 1.5: PointSpan wrappers
// ============================================================================

/// Returns true if point P lies strictly inside the circumsphere of the
/// simplex defined by vertices (v0, rest...).
template <PointSpan S, PointSpan... Rest>
    requires(std::same_as<S, Rest> && ...)
auto point_in_circumsphere(S P, S v0, Rest... rest) -> bool {
    return point_in_circumsphere(::zipper::VectorBase(P),
                                 ::zipper::VectorBase(v0),
                                 ::zipper::VectorBase(rest)...);
}

// ============================================================================
// Layer 1.5M: Matrix wrappers
// ============================================================================

/// Test whether point P lies inside the circumsphere of a simplex given
/// as a D×N matrix of vertex columns.
///
/// Calls the matrix `circumcenter_with_squared_radius(pts)` directly —
/// no variadic unpacking.
template <::zipper::concepts::Vector V, ::zipper::concepts::Matrix M>
auto point_in_circumsphere(const V &P, const M &pts) -> bool {
    auto [C, r2] = circumcenter_with_squared_radius(pts);
    using T = typename V::value_type;
    constexpr auto D = V::extents_type::static_extent(0);
    ::zipper::Vector<T, D> diff = C - P;
    return diff.template norm_powered<2>() < r2;
}

// ── Eigen overloads (convert to zipper) ─────────────────────────────

template <eigen::concepts::MatrixBaseDerived P,
          eigen::concepts::MatrixBaseDerived M>
auto point_in_circumsphere(const P &p, const M &S) -> bool {
    return point_in_circumsphere(eigen::as_zipper(p), eigen::as_zipper(S));
}

} // namespace balsa::geometry::simplex
#endif
