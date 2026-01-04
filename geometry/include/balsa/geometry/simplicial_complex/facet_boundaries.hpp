#if !defined(BALSA_GEOMETRY_SIMPLICIAL_COMPLEX_BOUNDARIES)
#define BALSA_GEOMETRY_SIMPLICIAL_COMPLEX_BOUNDARIES
#include "balsa/eigen/types.hpp"
#include "balsa/eigen/concepts/shape_types.hpp"

#include <set>


namespace balsa::geometry::simplicial_complex {

// returns column vectors of simplices one dimension lower than that of hte input
// Only for simplices
// return type for a k-simplex is a ColVectorsr<k-1> type;
template<eigen::concepts::ColVecsDCompatible CellType>
auto boundaries(const CellType &C) {


    constexpr static int CellSize = eigen::concepts::detail::compile_col_size<CellType>;
    constexpr static int FacetSize = (CellSize < 1) ? CellSize : (CellSize - 1);
    using FacetType = balsa::eigen::ColVectors<typename CellType::Scalar, FacetSize>;

    using Index = typename CellType::Scalar;

    using FacetArray = typename std::array<Index, FacetSize>;
    using CellArray = typename std::array<Index, CellSize>;

    using FacetVector = typename balsa::eigen::Vector<Index, FacetSize>;
    using CellVector = typename balsa::eigen::Vector<Index, CellSize>;

    std::set<std::array<Index, FacetSize>> facet_set;

    FacetArray f;
    Eigen::Map<FacetVector> fm(f.data());

    FacetArray f2;
    Eigen::Map<FacetVector> fm2(f2.data());
    CellArray c;
    Eigen::Map<CellVector> cm(c.data());
    for (int i = 0; i < C.cols(); ++i) {
        auto c = C.col(i);
        fm = c.template topRows<FacetSize>();
        for (int j = 0; j < CellSize; ++j) {
            fm2 = fm;
            std::sort(f2.begin(), f2.end());
            facet_set.insert(f2);

            auto unused = (j + FacetSize) % CellSize;
            fm(j % FacetSize) = c(unused);
        }
    }

    FacetType F(C.rows() - 1, facet_set.size());
    int idx = 0;
    for (auto &&f : facet_set) {
        F.col(idx++) = Eigen::Map<const FacetVector>(f.begin());
    }
    return F;
}


}// namespace balsa::geometry::simplicial_complex
#endif
