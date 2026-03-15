#if !defined(BALSA_GEOMETRY_SIMPLICIAL_COMPLEX_CIRCUMCENTERS_HPP)
#define BALSA_GEOMETRY_SIMPLICIAL_COMPLEX_CIRCUMCENTERS_HPP
#include "balsa/geometry/simplex/circumcenter.hpp"

namespace balsa::geometry::simplicial_complex {

/// Compute the circumcenter of each simplex in S, given vertex positions V.
///
/// @param V  Position matrix (D rows x num_vertices columns).
/// @param S  Simplex index matrix (K+1 rows x num_simplices columns),
///           integral-valued.
/// @return   Matrix (D rows x num_simplices columns) where each column
///           is the circumcenter of the corresponding simplex.
template<
  ::zipper::concepts::Matrix PosType,
  ::zipper::concepts::Matrix SimplexType>
    requires(std::is_integral_v<typename SimplexType::value_type>)
auto circumcenters(const PosType &V, const SimplexType &S) {

    using value_type = typename PosType::value_type;

    constexpr static zipper::index_type compile_rows = PosType::extents_type::static_extent(0);
    constexpr static zipper::index_type compile_cols = SimplexType::extents_type::static_extent(1);
    balsa::zipper::Matrix<value_type, compile_rows, compile_cols> C(V.extent(0), S.extent(1));

    for (zipper::index_type j = 0; j < S.extent(1); ++j) {
        auto s = S.col(j);
        auto c = C.col(j);
        c = simplex::circumcenter(V(::zipper::full_extent_t{}, s));
    }
    return C;
}
}// namespace balsa::geometry::simplicial_complex
#endif
