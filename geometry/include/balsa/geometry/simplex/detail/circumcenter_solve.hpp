#if !defined(BALSA_GEOMETRY_SIMPLEX_DETAIL_CIRCUMCENTER_SOLVE_HPP)
#define BALSA_GEOMETRY_SIMPLEX_DETAIL_CIRCUMCENTER_SOLVE_HPP

/// @file detail/circumcenter_solve.hpp
/// @brief Shared circumcenter solve logic for variadic and matrix paths.
///
/// Given a base vertex v0 and a D×K edge matrix E (where column i = v_{i+1} -
/// v0), solves for the circumcenter via the Gram-system approach:
///   1. Gram matrix G = E^T * E  (K×K, SPD for non-degenerate).
///   2. rhs_i = ||e_i||^2 / 2 = G(i,i) / 2.
///   3. LDLT-solve G * y = rhs.
///   4. Circumcenter = v0 + E * y.
///
/// The edge matrix convention is D×K (spatial dim = rows, edge count = cols),
/// matching the natural column-subtraction layout from a D×N point matrix.

#include <cmath>
#include <cstddef>
#include <tuple>

#include <zipper/Matrix.hpp>
#include <zipper/Vector.hpp>
#include <zipper/concepts/Matrix.hpp>
#include <zipper/concepts/Vector.hpp>
#include <zipper/utils/decomposition/ldlt.hpp>

namespace balsa::geometry::simplex::detail {

/// Circumcenter from a base vertex and edge matrix.
///
/// @param v0  Base vertex (D-dimensional zipper Vector).
/// @param E   D×K edge matrix (cols = edge vectors from v0).
/// @param centroid_fallback  D-dimensional centroid to use on degenerate input.
/// @return    Circumcenter as a zipper Vector<T, D>.
template <::zipper::concepts::Vector V,
          ::zipper::concepts::Matrix M,
          ::zipper::concepts::Vector Centroid>
auto circumcenter_from_edges(const V &v0,
                             const M &E,
                             const Centroid &centroid_fallback)
    -> ::zipper::Vector<typename V::value_type,
                        V::extents_type::static_extent(0)> {
    using T = typename V::value_type;
    constexpr auto D = V::extents_type::static_extent(0);

    // Gram matrix G = E^T * E  (K×K).
    auto G = (E.transpose() * E).eval();

    // RHS: rhs_i = ||e_i||^2 / 2 = G(i,i) / 2.
    constexpr auto K = decltype(G)::extents_type::static_extent(0);
    ::zipper::Vector<T, K> rhs(G.extent(0));
    for (std::size_t i = 0; i < G.extent(0); ++i) { rhs(i) = G(i, i) / T(2); }

    // Solve G * y = rhs via LDLT.
    auto solve_result = ::zipper::utils::decomposition::ldlt_solve(G, rhs);

    if (solve_result) {
        // Circumcenter = v0 + E * y.
        return (v0 + E * *solve_result).eval();
    } else {
        // Degenerate: fall back to centroid.
        return ::zipper::Vector<T, D>(centroid_fallback);
    }
}

/// Circumcenter and squared circumradius from a base vertex and edge matrix.
///
/// @param v0  Base vertex (D-dimensional zipper Vector).
/// @param E   D×K edge matrix (cols = edge vectors from v0).
/// @param centroid_fallback  D-dimensional centroid for degenerate fallback.
/// @return    Tuple of (circumcenter, squared_circumradius).
template <::zipper::concepts::Vector V,
          ::zipper::concepts::Matrix M,
          ::zipper::concepts::Vector Centroid>
auto circumcenter_with_squared_radius_from_edges(
    const V &v0,
    const M &E,
    const Centroid &centroid_fallback) {
    auto C = circumcenter_from_edges(v0, E, centroid_fallback);
    using T = typename V::value_type;
    constexpr auto D = V::extents_type::static_extent(0);
    ::zipper::Vector<T, D> diff = C - v0;
    T r2 = diff.template norm_powered<2>();
    return std::make_tuple(std::move(C), r2);
}

} // namespace balsa::geometry::simplex::detail
#endif
