#pragma once
#include "types.hpp"
#include <zipper/expression/nullary/StlMDArray.hpp>


namespace balsa::zipper {

// Convert an STL container (e.g., std::vector<T>, std::array<T,N>,
// std::vector<std::array<T,D>>) to an owned zipper expression.
// Uses StlMDArray to view the STL data and .eval() to materialize.
auto stl2zipper(auto const &M) {
    return ::zipper::expression::nullary::StlMDArray<const std::decay_t<decltype(M)> &>(M).eval();
}

}// namespace balsa::zipper
