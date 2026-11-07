#if !defined(BALSA_GEOMETRY_SIMPLICIAL_COMPLEX_DETAIL_ORIENTED_FACET_HPP)
#define BALSA_GEOMETRY_SIMPLICIAL_COMPLEX_DETAIL_ORIENTED_FACET_HPP
#include "balsa/geometry/simplicial_complex/detail/combinations.hpp"
#include "balsa/geometry/simplicial_complex/detail/concepts.hpp"

#include <zipper/VectorBase.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <utility>

namespace balsa::geometry::simplicial_complex {

// A face's canonical orientation is its sorted vertex order.
namespace detail {
    // Facet of simplex s opposite vertex `opposite`: sorted vertices, and
    // whether its orientation in s is opposite to theirs.
    template <std::size_t N, typename Index, typename CellType>
    auto oriented_facet(const CellType &C, std::size_t s, std::size_t opposite)
        -> std::pair<std::array<Index, N - 1>, bool> {
        static constexpr auto facets = opposite_facets<N>();
        std::array<Index, N - 1> key{};
        ::zipper::VectorBase{key} = C.slice(facets[opposite], s);
        bool odd = opposite % 2 == 1;
        for (std::size_t i = 0; i < N - 1; ++i) {
            for (std::size_t j = i + 1; j < N - 1; ++j) odd ^= key[i] > key[j];
        }
        std::ranges::sort(key);
        return {key, odd};
    }
} // namespace detail

} // namespace balsa::geometry::simplicial_complex
#endif
