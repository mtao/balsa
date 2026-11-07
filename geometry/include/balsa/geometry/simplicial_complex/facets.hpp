#if !defined(BALSA_GEOMETRY_SIMPLICIAL_COMPLEX_FACETS_HPP)
#define BALSA_GEOMETRY_SIMPLICIAL_COMPLEX_FACETS_HPP
#include "balsa/geometry/simplicial_complex/detail/oriented_facet.hpp"
#include "balsa/geometry/simplicial_complex/faces.hpp"

namespace balsa::geometry::simplicial_complex {

/// Distinct facets of C: the row basis of boundary_matrix.
template <detail::FixedCells CellType>
auto facets(const CellType &C) {
    return faces<CellType::extents_type::static_extent(0) - 1>(C);
}

} // namespace balsa::geometry::simplicial_complex
#endif
