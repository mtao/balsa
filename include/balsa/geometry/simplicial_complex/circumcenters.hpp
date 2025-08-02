#if !defined(BALSA_GEOMETRY_SIMPLICIAL_COMPLEX_CIRCUMCENTERS_HPP)
#define BALSA_GEOMETRY_SIMPLICIAL_COMPLEX_CIRCUMCENTERS_HPP
#include "balsa/geometry/simplex/circumcenter.hpp"

namespace balsa::geometry::simplicial_complex {
template<
  ::zipper::concepts::MatrixBaseDerived PosType,
  ::zipper::concepts::MatrixBaseDerived SimplexType>
    requires(std::is_integral_v<typename SimplexType::value_type>)
auto circumcenters(const PosType &V, const SimplexType &S) {

    using value_type = typename PosType::value_type;

    auto cols = S.cols();
    constexpr static zipper::index_type compile_cols = SimplexType::extents_type::static_extent(1);
    balsa::zipper::Vector<value_type, compile_cols> C(cols);

    // tbb::parallel_for(int(0), int(S.cols()), [&](int j) {
    //     auto s = S.col(j);
    //     C(j) = volume(V(Eigen::all, s));
    // });
    for (zipper::index_type j = 0; j < S.extent(1); ++j) {
        auto s = S.col(j);
        C(j) = simplex::circumcenter(V(::zipper::full_extent_t{}, s));
    }
    return C;
}
}// namespace balsa::geometry::simplicial_complex
#endif
