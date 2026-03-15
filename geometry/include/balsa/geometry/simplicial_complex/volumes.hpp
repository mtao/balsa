#if !defined(BALSA_GEOMETRY_SIMPLICIAL_COMPLEX_VOLUMES_HPP)
#define BALSA_GEOMETRY_SIMPLICIAL_COMPLEX_VOLUMES_HPP
#include "balsa/geometry/simplex/volume.hpp"

namespace balsa::geometry::simplicial_complex {
template<
  ::zipper::concepts::Matrix PosType,
  ::zipper::concepts::Matrix SimplexType>
    requires(std::is_integral_v<typename SimplexType::value_type>)
auto volumes(const PosType &V, const SimplexType &S) {

    /*
    constexpr static int cols = eigen::concepts::detail::compile_col_size<SimplexType>;
    using RetType = eigen::RowVector<typename PosType::Scalar, cols>;

    RetType R(S.cols());

    for (int j = 0; j < R.size(); ++j) {
        R(j) = simplex::volume(V(Eigen::all, S.col(j)));
    }
    return R;
    */
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
        C(j) = simplex::volume(V(::zipper::full_extent_t{}, s));
    }
    return C;
}
}// namespace balsa::geometry::simplicial_complex
#endif
