#if !defined(BALSA_GEOMETRY_SIMPLEX_CONCEPTS_HPP)
#define BALSA_GEOMETRY_SIMPLEX_CONCEPTS_HPP

/// @file concepts.hpp
/// @brief Shared geometric concepts for simplex computations.
///
/// Provides `PointSpan` and related traits used throughout the simplex module
/// (volume, circumcenter, quality metrics, etc.).

#include <array>
#include <concepts>
#include <cstddef>
#include <span>
#include <type_traits>
#include <utility>

namespace balsa::geometry::simplex {

/// Scalar types usable in simplex computations.
template <typename T>
concept SimplexScalar = std::floating_point<T>;

/// A fixed-size const span of floating-point values representing a point.
template <typename S>
concept PointSpan = requires {
    typename S::element_type;
    requires SimplexScalar<std::remove_const_t<typename S::element_type>>;
    { S::extent } -> std::convertible_to<std::size_t>;
    requires S::extent != std::dynamic_extent;
};

/// Extract the scalar type from a span.
template <PointSpan S>
using scalar_t = std::remove_const_t<typename S::element_type>;

/// Extract the spatial dimension from a span.
template <PointSpan S>
inline constexpr std::size_t dim_v = S::extent;

/// A type that provides `.as_tuple()` for use with `std::apply`.
///
/// Satisfied by `IncidentData<T, N, Storage>` (fixed extent) and any
/// user-defined type providing the same protocol.  The returned tuple
/// must contain elements convertible to `::zipper::VectorBase` via CTAD
/// for use with the Layer 1.5ID simplex overloads.
template <typename T>
concept HasAsTuple = requires(const T &t) {
    { t.as_tuple() };
};

/// Convert a zipper vector to a std::array.
///
/// Used at the L1.5 PointSpan boundary to bridge zipper types back to
/// plain arrays.  Zipper vectors don't expose a native `to_array()`.
template <typename ZipperVec>
auto to_array(const ZipperVec &v) {
    using T = typename ZipperVec::value_type;
    constexpr auto N = ZipperVec::extents_type::static_extent(0);
    return [&]<std::size_t... I>(std::index_sequence<I...>) {
        return std::array<T, N>{v(I)...};
    }(std::make_index_sequence<N>{});
}

} // namespace balsa::geometry::simplex
#endif
