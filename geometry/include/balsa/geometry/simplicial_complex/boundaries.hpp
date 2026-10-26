#if !defined(BALSA_GEOMETRY_SIMPLICIAL_COMPLEX_BOUNDARIES)
#define BALSA_GEOMETRY_SIMPLICIAL_COMPLEX_BOUNDARIES
#include "balsa/tensor_types.hpp"
#include "balsa/zipper/types.hpp"
#include <zipper/concepts/Matrix.hpp>
#include <zipper/utils/extents/extent_arithmetic.hpp>

#include <set>

namespace balsa::geometry::simplicial_complex {

// returns column vectors of simplices one dimension lower than that of hte input
// Only for simplices
// return type for a k-simplex is a ColVectorsr<k-1> type;

template<::zipper::concepts::Matrix CellType>
auto boundaries(const CellType &C) {

    constexpr static int CellSize = CellType::extents_type::static_extent(0);
    constexpr static int FacetSize = ::zipper::utils::extents::minus(CellSize, 1);
    using FacetType = balsa::zipper::ColVectors<typename CellType::value_type, FacetSize>;

    using Index = typename CellType::value_type;

    using FacetArray = typename std::array<Index, FacetSize>;
    using CellArray = typename std::array<Index, CellSize>;

    using FacetVector = typename balsa::zipper::Vector<Index, FacetSize>;
    using CellVector = typename balsa::zipper::Vector<Index, CellSize>;

    std::set<std::array<Index, FacetSize>> facet_set;

    FacetArray f;
    typename FacetVector::span_type fm(f);

    FacetArray f2;
    typename FacetVector::span_type fm2(f2);
    CellArray c;
    typename CellVector::span_type cm(c);
    for (index_type i = 0; i < C.cols(); ++i) {
        auto c = C.col(i);
        fm = c.template head<FacetSize>();
        for (int j = 0; j < CellSize; ++j) {
            fm2 = fm;
            std::ranges::sort(f2.begin(), f2.end());
            facet_set.insert(f2);

            auto unused = (j + FacetSize) % CellSize;
            fm(j % FacetSize) = c(unused);
        }
    }

    FacetType F(C.rows() - 1, facet_set.size());
    int idx = 0;
    for (auto &&f : facet_set) {
        F.col(idx++) = typename FacetVector::const_span_type(std::span(f));
    }
    return F;
}

}// namespace balsa::geometry::simplicial_complex
#endif
