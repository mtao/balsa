#if !defined(BALSA_GEOMETRY_SIMPLICIAL_COMPLEX_DETAIL_COMBINATIONS_HPP)
#define BALSA_GEOMETRY_SIMPLICIAL_COMPLEX_DETAIL_COMBINATIONS_HPP
#include <algorithm>
#include <array>
#include <cstddef>
#include <ranges>

namespace balsa::geometry::simplicial_complex::detail {

consteval auto binomial(std::size_t n, std::size_t k) -> std::size_t {
    if (k > n) return 0;
    std::size_t result = 1;
    for (std::size_t i = 1; i <= k; ++i) result = result * (n - k + i) / i;
    return result;
}

// K-element subsets of {0, ..., N-1}, each increasing.  Distinct
// permutations of a K-true selector are exactly the subsets.
template <std::size_t N, std::size_t K>
consteval auto combinations()
    -> std::array<std::array<std::size_t, K>, binomial(N, K)> {
    std::array<bool, N> selected{};
    std::ranges::fill(selected | std::views::drop(N - K), true);
    std::array<std::array<std::size_t, K>, binomial(N, K)> result{};
    for (auto &combination : result) {
        std::size_t k = 0;
        for (std::size_t v = 0; v < N; ++v) {
            if (selected[v]) combination[k++] = v;
        }
        std::ranges::next_permutation(selected);
    }
    return result;
}

// Row i: the vertices of the facet opposite vertex i, in order.
template <std::size_t N>
consteval auto opposite_facets() -> std::array<std::array<std::size_t, N - 1>, N> {
    std::array<std::array<std::size_t, N - 1>, N> result{};
    for (std::size_t i = 0; i < N; ++i) {
        std::size_t k = 0;
        for (std::size_t v = 0; v < N; ++v) {
            if (v != i) result[i][k++] = v;
        }
    }
    return result;
}

} // namespace balsa::geometry::simplicial_complex::detail
#endif
