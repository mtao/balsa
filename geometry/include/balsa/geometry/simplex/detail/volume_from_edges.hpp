#if !defined(BALSA_GEOMETRY_SIMPLEX_DETAIL_VOLUME_FROM_EDGES_HPP)
#define BALSA_GEOMETRY_SIMPLEX_DETAIL_VOLUME_FROM_EDGES_HPP

/// @file detail/volume_from_edges.hpp
/// @brief Shared volume computation from edge matrices.
///
/// Given a D×K edge matrix E (where column i = v_{i+1} - v0):
///   - **Signed volume** (K == D): det(E) / K!
///   - **Unsigned volume** (K <= D): sqrt(det(E^T * E)) / K!
///
/// The edge matrix convention is D×K (spatial dim = rows, edge count = cols).

#include <cmath>
#include <cstddef>

#include <zipper/concepts/Matrix.hpp>
#include <zipper/utils/determinant.hpp>

namespace balsa::geometry::simplex::detail {

/// Runtime-callable constexpr factorial.
/// Quiver's combinatorics::factorial is consteval-only; volume
/// kernels need a runtime version when dimensions are dynamic.
constexpr auto factorial(std::size_t n) -> std::size_t {
    std::size_t r = 1;
    for (std::size_t i = 2; i <= n; ++i) { r *= i; }
    return r;
}

/// Signed volume from a square D×D edge matrix.
///
/// Requires K == D (full-rank simplex: D+1 vertices in D-space).
/// Returns det(E) / D!.
template <::zipper::concepts::Matrix M>
auto volume_signed_from_edges(const M &E) ->
    typename std::decay_t<M>::value_type {
    using T = typename std::decay_t<M>::value_type;
    const auto K = E.extent(1);
    return ::zipper::utils::determinant(E) / T(factorial(K));
}

/// Unsigned volume from a D×K edge matrix.
///
/// For D == K (square): |det(E)| / K!
/// For D > K (embedded): sqrt(det(E^T * E)) / K!
template <::zipper::concepts::Matrix M>
auto volume_unsigned_from_edges(const M &E) ->
    typename std::decay_t<M>::value_type {
    using T = typename std::decay_t<M>::value_type;
    constexpr auto D = std::decay_t<M>::extents_type::static_extent(0);
    constexpr auto K = std::decay_t<M>::extents_type::static_extent(1);
    const auto k = E.extent(1);

    if constexpr (D != std::dynamic_extent && K != std::dynamic_extent
                  && D == K) {
        // Square: |det(E)| / K!
        return std::abs(::zipper::utils::determinant(E)) / T(factorial(k));
    } else {
        // Embedded or dynamic: Gram determinant approach.
        auto gram = (E.transpose() * E).eval();
        T gram_det = ::zipper::utils::determinant(gram);
        return std::sqrt(std::max(gram_det, T{0})) / T(factorial(k));
    }
}

} // namespace balsa::geometry::simplex::detail
#endif
