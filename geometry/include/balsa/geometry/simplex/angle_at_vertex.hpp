#if !defined(BALSA_GEOMETRY_SIMPLEX_ANGLE_AT_VERTEX_HPP)
#define BALSA_GEOMETRY_SIMPLEX_ANGLE_AT_VERTEX_HPP

/// @file angle_at_vertex.hpp
/// @brief Angle at a vertex of a triangle (2-simplex), in radians.
///
/// Two-layer design:
///   - **Layer 1 (zipper-concepts core)**: constrained on
///   `::zipper::concepts::Vector`.
///   - **Layer 1.5 (PointSpan wrappers)**: thin overloads converting spans.
///
/// `angle_at_vertex(v, a, b)` computes the angle at vertex `v`
/// in the triangle (v, a, b), i.e. the angle between edges va and vb.
///
/// The computation is `atan2`-based for numerical stability:
///   - **2D (dim == 2)**: signed angle in (-pi, pi] via `atan2(cross2d, dot)`.
///   - **3D (dim == 3)**: unsigned angle in [0, pi] via `atan2(|cross3d|,
///   dot)`.
///   - **dim >= 4**: unsigned angle in [0, pi] via `atan2(sin_norm, dot)`
///     using the Gram identity `|va×vb|² = |va|²|vb|² - (va·vb)²`.
///
/// Returns 0 for degenerate configurations (zero-length edges).

#include <balsa/geometry/simplex/concepts.hpp>
#include <cmath>
#include <span>

#include <zipper/Vector.hpp>
#include <zipper/VectorBase.hpp>
#include <zipper/concepts/Vector.hpp>

namespace balsa::geometry::simplex {

// ============================================================================
// Layer 1: zipper-concepts core
// ============================================================================

namespace detail {

    /// 2D specialization: signed angle via atan2(cross2d, dot).
    template <::zipper::concepts::Vector V>
        requires(V::extents_type::static_extent(0) == 2)
    auto angle_at_vertex_impl(const V &va, const V &vb) ->
        typename V::value_type {
        using T = typename V::value_type;
        T cross2d = va(0) * vb(1) - va(1) * vb(0);
        T dot = va.dot(vb);
        return std::atan2(cross2d, dot);
    }

    /// 3D specialization: unsigned angle via atan2(|cross3d|, dot).
    template <::zipper::concepts::Vector V>
        requires(V::extents_type::static_extent(0) == 3)
    auto angle_at_vertex_impl(const V &va, const V &vb) ->
        typename V::value_type {
        using T = typename V::value_type;
        T cross_norm = va.cross(vb).norm();
        T dot = va.dot(vb);
        return std::atan2(cross_norm, dot);
    }

    /// General dim >= 4: unsigned angle via Gram identity.
    /// |sin(θ)| = sqrt(|va|²|vb|² - (va·vb)²) / (|va||vb|)
    template <::zipper::concepts::Vector V>
        requires(V::extents_type::static_extent(0) >= 4)
    auto angle_at_vertex_impl(const V &va, const V &vb) ->
        typename V::value_type {
        using T = typename V::value_type;
        T dot = va.dot(vb);
        T sq_va = va.dot(va);
        T sq_vb = vb.dot(vb);
        T sin_norm = std::sqrt(std::max(sq_va * sq_vb - dot * dot, T{0}));
        return std::atan2(sin_norm, dot);
    }

} // namespace detail

/// Angle (in radians) at vertex `v` in triangle (v, a, b).
///
/// In 2D, returns a signed angle in (-pi, pi].
/// In 3D+, returns an unsigned angle in [0, pi].
/// Returns 0 for degenerate configurations (zero-length edges).
template <::zipper::concepts::Vector V>
    requires(V::extents_type::static_extent(0) >= 2)
auto angle_at_vertex(const V &v, const V &a, const V &b) ->
    typename V::value_type {
    using T = typename V::value_type;
    constexpr auto N = V::extents_type::static_extent(0);

    ::zipper::Vector<T, N> va = a - v;
    ::zipper::Vector<T, N> vb = b - v;

    T len_va = va.norm();
    T len_vb = vb.norm();
    if (len_va <= T{0} || len_vb <= T{0}) return T{0};

    return detail::angle_at_vertex_impl(va, vb);
}

// ============================================================================
// Layer 1.5: PointSpan wrappers
// ============================================================================

/// Angle (in radians) at vertex `v` in triangle (v, a, b)
/// (PointSpan wrapper).
template <PointSpan S>
    requires(dim_v<S> >= 2)
auto angle_at_vertex(S v, S a, S b) -> scalar_t<S> {
    return angle_at_vertex(::zipper::VectorBase(v),
                           ::zipper::VectorBase(a),
                           ::zipper::VectorBase(b));
}

} // namespace balsa::geometry::simplex
#endif
