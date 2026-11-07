#if !defined(BALSA_GEOMETRY_SIMPLICIAL_COMPLEX_DETAIL_CONCEPTS_HPP)
#define BALSA_GEOMETRY_SIMPLICIAL_COMPLEX_DETAIL_CONCEPTS_HPP
#include <zipper/concepts/Matrix.hpp>

#include <cstddef>
#include <span>

namespace balsa::geometry::simplicial_complex::detail {

// Simplex columns with a compile-time vertex count.
template <typename CellType>
concept FixedCells = ::zipper::concepts::Matrix<CellType>
                     && CellType::extents_type::static_extent(0) != std::dynamic_extent
                     && CellType::extents_type::static_extent(0) >= 2;

} // namespace balsa::geometry::simplicial_complex::detail
#endif
