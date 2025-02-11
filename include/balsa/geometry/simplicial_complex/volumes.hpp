#if !defined(BALSA_GEOMETRY_SIMPLICIAL_COMPLEX_VOLUMES_HPP)
#define BALSA_GEOMETRY_SIMPLICIAL_COMPLEX_VOLUMES_HPP
#include "../simplex/volume.hpp"

namespace balsa::geometry::simplicial_complex {
template<
  eigen::concepts::ColVecsDCompatible PosType,
  eigen::concepts::ColVecsDCompatible SimplexType>
auto volumes(const PosType &V, const SimplexType &S) {

    constexpr static int cols = eigen::concepts::detail::compile_col_size<SimplexType>;
    using RetType = eigen::RowVector<typename PosType::Scalar, cols>;

    RetType R(S.cols());

    for (int j = 0; j < R.size(); ++j) {
        R(j) = simplex::volume(V(Eigen::all, S.col(j)));
    }
    return R;
}
}// namespace balsa::geometry::simplicial_complex
#endif
