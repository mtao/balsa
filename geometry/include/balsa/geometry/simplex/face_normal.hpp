#if !defined(BALSA_GEOMETRY_SIMPLEX_FACE_NORMAL_HPP)
#define BALSA_GEOMETRY_SIMPLEX_FACE_NORMAL_HPP

/// @file face_normal.hpp
/// @brief Triangle face normal computation (unnormalized and unit).
///
/// Four-layer design:
///   - **Layer 1 (zipper-concepts core)**: constrained on
///   `::zipper::concepts::Vector`.
///   - **Layer 1.5 (PointSpan wrappers)**: thin overloads converting
///     `std::span<const T, N>` to zipper via `VectorBase(span)` CTAD.
///   - **Layer 1.5ID (HasAsTuple wrappers)**: for IncidentData and similar.
///
/// `face_normal(v0, v1, v2)` computes the unnormalized cross product
/// `(v1-v0) x (v2-v0)`.  Its magnitude equals twice the triangle area,
/// making it useful for area-weighted normal accumulation.
///
/// For 2D inputs (D == 2), the vertices are lifted to 3D (z = 0)
/// before computing the cross product, yielding a normal with only a
/// z-component proportional to twice the signed area.
///
/// `face_unit_normal(v0, v1, v2)` returns the normalized result, or
/// the zero vector for degenerate (zero-area) triangles.

#include <balsa/geometry/simplex/concepts.hpp>

#include <array>
#include <cmath>
#include <cstddef>
#include <span>
#include <tuple>
#include <utility>

#include <zipper/Vector.hpp>
#include <zipper/VectorBase.hpp>
#include <zipper/concepts/Vector.hpp>

namespace balsa::geometry::simplex {

// ============================================================================
// Layer 1: zipper-concepts core
// ============================================================================

namespace detail {

    /// Lift a D-dimensional zipper vector to 3D, padding with zeros.
    template <::zipper::concepts::Vector V>
    auto lift_to_3d(const V &v) -> ::zipper::Vector<typename V::value_type, 3> {
        using T = typename V::value_type;
        constexpr auto D = V::extents_type::static_extent(0);
        ::zipper::Vector<T, 3> result;
        for (std::size_t d = 0; d < std::min(D, std::size_t{3}); ++d) {
            result(d) = v(d);
        }
        return result;
    }

} // namespace detail

/// Unnormalized face normal of a triangle (zipper-concepts core).
///
/// Computes `(v1 - v0) x (v2 - v0)`.  The result is a 3D vector
/// whose magnitude equals twice the triangle area.
///
/// For 2D inputs (D == 2), the vertices are lifted to 3D (z = 0)
/// before the cross product.  For 3D inputs, the cross product is
/// computed directly.
///
/// @param v0, v1, v2  Triangle vertices as zipper vectors.
/// @return A `::zipper::Vector<T, 3>` with the unnormalized normal.
template <::zipper::concepts::Vector V>
    requires(V::extents_type::static_extent(0) >= 2
             && V::extents_type::static_extent(0) <= 3)
auto face_normal(const V &v0, const V &v1, const V &v2)
    -> ::zipper::Vector<typename V::value_type, 3> {
    constexpr auto D = V::extents_type::static_extent(0);

    if constexpr (D == 3) {
        auto e1 = v1 - v0;
        auto e2 = v2 - v0;
        return e1.cross(e2);
    } else {
        // D == 2: lift to 3D (z = 0), then cross product.
        auto e1 = detail::lift_to_3d(v1 - v0);
        auto e2 = detail::lift_to_3d(v2 - v0);
        return e1.cross(e2);
    }
}

/// Unit face normal of a triangle (zipper-concepts core).
///
/// Returns the normalized face normal, or the zero vector for
/// degenerate (zero-area) triangles.
///
/// @param v0, v1, v2  Triangle vertices as zipper vectors.
/// @return A `::zipper::Vector<T, 3>` unit normal or zero vector.
template <::zipper::concepts::Vector V>
    requires(V::extents_type::static_extent(0) >= 2
             && V::extents_type::static_extent(0) <= 3)
auto face_unit_normal(const V &v0, const V &v1, const V &v2)
    -> ::zipper::Vector<typename V::value_type, 3> {
    using T = typename V::value_type;
    auto n = face_normal(v0, v1, v2);
    T len = n.norm();
    if (len > T{0}) { n /= len; }
    return n;
}

// ============================================================================
// Layer 1.5: PointSpan wrappers
// ============================================================================

/// Unnormalized face normal (PointSpan wrapper).
template <PointSpan S>
    requires(dim_v<S> >= 2 && dim_v<S> <= 3)
auto face_normal(S v0, S v1, S v2) {
    return face_normal(::zipper::VectorBase(v0),
                       ::zipper::VectorBase(v1),
                       ::zipper::VectorBase(v2));
}

/// Unit face normal (PointSpan wrapper).
template <PointSpan S>
    requires(dim_v<S> >= 2 && dim_v<S> <= 3)
auto face_unit_normal(S v0, S v1, S v2) {
    return face_unit_normal(::zipper::VectorBase(v0),
                            ::zipper::VectorBase(v1),
                            ::zipper::VectorBase(v2));
}

// ============================================================================
// Layer 1.5ID: HasAsTuple wrappers (IncidentData and similar)
// ============================================================================

/// Unnormalized face normal (HasAsTuple wrapper).
///
/// Returns `std::array<T, 3>` for pipe-system compatibility.
template <HasAsTuple ID>
auto face_normal(const ID &id) {
    return std::apply(
        [](const auto &v0, const auto &v1, const auto &v2) {
            auto n = face_normal(::zipper::VectorBase(v0),
                                 ::zipper::VectorBase(v1),
                                 ::zipper::VectorBase(v2));
            return to_array(n);
        },
        id.as_tuple());
}

/// Unit face normal (HasAsTuple wrapper).
///
/// Returns `std::array<T, 3>` for pipe-system compatibility.
template <HasAsTuple ID>
auto face_unit_normal(const ID &id) {
    return std::apply(
        [](const auto &v0, const auto &v1, const auto &v2) {
            auto n = face_unit_normal(::zipper::VectorBase(v0),
                                      ::zipper::VectorBase(v1),
                                      ::zipper::VectorBase(v2));
            return to_array(n);
        },
        id.as_tuple());
}

} // namespace balsa::geometry::simplex
#endif
