#if !defined(BALSA_GEOMETRY_SIMPLICIAL_COMPLEX_BOUNDARY_HPP)
#define BALSA_GEOMETRY_SIMPLICIAL_COMPLEX_BOUNDARY_HPP
#include "balsa/geometry/simplicial_complex/detail/oriented_facet.hpp"
#include "balsa/zipper/types.hpp"

#include <zipper/VectorBase.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <type_traits>
#include <utility>
#include <vector>

namespace balsa::geometry::simplicial_complex {

/// Boundary of C, i.e. the boundary operator applied to the sum of its
/// simplices: facets in exactly one simplex, oriented as its face (outward
/// for positively oriented simplices).  Unordered.
template <detail::FixedCells CellType>
auto boundary(const CellType &C) {
    using Index = std::remove_cv_t<typename CellType::value_type>;
    constexpr std::size_t N = CellType::extents_type::static_extent(0);
    constexpr std::size_t K = N - 1;

    std::vector<std::pair<std::array<Index, K>, bool>> facets;
    facets.reserve(N * C.extent(1));
    for (std::size_t s = 0; s < C.extent(1); ++s) {
        for (std::size_t opposite = 0; opposite < N; ++opposite) {
            facets.push_back(detail::oriented_facet<N, Index>(C, s, opposite));
        }
    }
    std::ranges::sort(facets, {}, &decltype(facets)::value_type::first);

    std::vector<std::array<Index, K>> result;
    for (auto it = facets.begin(); it != facets.end();) {
        const auto end = std::ranges::find_if(
            it, facets.end(), [&](const auto &f) { return f.first != it->first; });
        if (end - it == 1) {
            auto oriented = it->first;
            if (it->second) {
                if constexpr (K >= 2) std::swap(oriented[0], oriented[1]);
            }
            result.push_back(oriented);
        }
        it = end;
    }

    balsa::zipper::ColVectors<Index, K> B(K, result.size());
    for (std::size_t f = 0; f < result.size(); ++f) B.col(f) = ::zipper::VectorBase(result[f]);
    return B;
}

} // namespace balsa::geometry::simplicial_complex
#endif
