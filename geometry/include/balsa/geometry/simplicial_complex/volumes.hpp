#if !defined(BALSA_GEOMETRY_SIMPLICIAL_COMPLEX_VOLUMES_HPP)
#define BALSA_GEOMETRY_SIMPLICIAL_COMPLEX_VOLUMES_HPP
#include "balsa/geometry/simplex/volume.hpp"
#include <zipper/Vector.hpp>

namespace balsa::geometry::simplicial_complex {

/// Compute the volume of each simplex in S, given vertex positions V.
///
/// @param V  Position matrix (D rows x num_vertices columns).
/// @param S  Simplex index matrix (K+1 rows x num_simplices columns),
///           integral-valued.
/// @return   Vector of length num_simplices containing per-simplex volumes.
///           Signed when D+1 == K+1 (i.e. full-rank simplices), unsigned
///           otherwise.
template<
  ::zipper::concepts::Matrix PosType,
  ::zipper::concepts::Matrix SimplexType>
    requires(std::is_integral_v<typename SimplexType::value_type>)
auto volumes(const PosType &V, const SimplexType &S) {

    using value_type = typename PosType::value_type;

    constexpr static ::zipper::index_type compile_cols = SimplexType::extents_type::static_extent(1);
    ::zipper::Vector<value_type, compile_cols> C(S.extent(1));

    for (::zipper::index_type j = 0; j < S.extent(1); ++j) {
        auto s = S.col(j);
        C(j) = simplex::volume(V(::zipper::full_extent_t{}, s));
    }
    return C;
}
}// namespace balsa::geometry::simplicial_complex
#endif
