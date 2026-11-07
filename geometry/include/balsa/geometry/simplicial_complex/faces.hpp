#if !defined(BALSA_GEOMETRY_SIMPLICIAL_COMPLEX_FACES_HPP)
#define BALSA_GEOMETRY_SIMPLICIAL_COMPLEX_FACES_HPP
#include "balsa/zipper/types.hpp"

#include "balsa/geometry/simplicial_complex/detail/combinations.hpp"
#include "balsa/geometry/simplicial_complex/detail/concepts.hpp"

#include <zipper/VectorBase.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <type_traits>
#include <vector>

namespace balsa::geometry::simplicial_complex {
// Simplices are columns of vertex indices (N rows for (N-1)-simplices).

/// Distinct K-vertex faces of C, vertices sorted, faces in lexicographic
/// order.
template <std::size_t K, detail::FixedCells CellType>
    requires(K >= 1 && K <= CellType::extents_type::static_extent(0))
auto faces(const CellType &C) {
    using Index = std::remove_cv_t<typename CellType::value_type>;
    constexpr std::size_t N = CellType::extents_type::static_extent(0);
    static constexpr auto subsets = detail::combinations<N, K>();

    std::vector<std::array<Index, K>> found;
    found.reserve(subsets.size() * C.extent(1));
    for (std::size_t s = 0; s < C.extent(1); ++s) {
        for (const auto &subset : subsets) {
            std::array<Index, K> face{};
            ::zipper::VectorBase{face} = C.slice(subset, s);
            std::ranges::sort(face);
            found.push_back(face);
        }
    }
    std::ranges::sort(found);
    const auto [first, last] = std::ranges::unique(found);
    found.erase(first, last);

    balsa::zipper::ColVectors<Index, K> F(K, found.size());
    for (std::size_t f = 0; f < found.size(); ++f) F.col(f) = ::zipper::VectorBase(found[f]);
    return F;
}

} // namespace balsa::geometry::simplicial_complex
#endif
