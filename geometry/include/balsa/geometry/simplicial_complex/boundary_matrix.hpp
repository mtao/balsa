#if !defined(BALSA_GEOMETRY_SIMPLICIAL_COMPLEX_BOUNDARY_MATRIX_HPP)
#define BALSA_GEOMETRY_SIMPLICIAL_COMPLEX_BOUNDARY_MATRIX_HPP
#include "balsa/geometry/simplicial_complex/detail/oriented_facet.hpp"

#include <zipper/COOMatrix.hpp>
#include <zipper/VectorBase.hpp>
#include <zipper/concepts/Matrix.hpp>

#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
#include <type_traits>
#include <utility>
#include <vector>

namespace balsa::geometry::simplicial_complex {

/// Boundary operator of C: rows are the facets `F` (e.g. facets(C)), columns
/// the simplices, entries +-1 relative to each facet's sorted orientation.
/// F must contain every facet of C.  Its transpose is the DEC exterior
/// derivative.
template <typename T = double, detail::FixedCells CellType, ::zipper::concepts::Matrix FacetType>
auto boundary_matrix(const CellType &C, const FacetType &F)
    -> ::zipper::COOMatrix<T, std::dynamic_extent, std::dynamic_extent> {
    using Index = std::remove_cv_t<typename CellType::value_type>;
    constexpr std::size_t N = CellType::extents_type::static_extent(0);
    assert(F.extent(0) == N - 1);

    // Sorted facet -> row.
    std::vector<std::pair<std::array<Index, N - 1>, std::size_t>> rows;
    rows.reserve(F.extent(1));
    for (std::size_t f = 0; f < F.extent(1); ++f) {
        std::array<Index, N - 1> key{};
        ::zipper::VectorBase{key} = F.col(f).template cast<Index>();
        std::ranges::sort(key);
        rows.emplace_back(key, f);
    }
    std::ranges::sort(rows);

    ::zipper::COOMatrix<T, std::dynamic_extent, std::dynamic_extent> D(F.extent(1),
                                                                         C.extent(1));
    for (std::size_t s = 0; s < C.extent(1); ++s) {
        for (std::size_t opposite = 0; opposite < N; ++opposite) {
            const auto [key, odd] = detail::oriented_facet<N, Index>(C, s, opposite);
            const auto it = std::ranges::lower_bound(rows, key, {}, &decltype(rows)::value_type::first);
            assert(it != rows.end() && it->first == key);
            D.emplace(it->second, s) = odd ? T(-1) : T(1);
        }
    }
    D.compress();
    return D;
}

} // namespace balsa::geometry::simplicial_complex
#endif
