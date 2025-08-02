#pragma once
#include "types.hpp"
#include "balsa/eigen/stl2eigen.hpp"
#include "balsa/eigen/zipper_compat.hpp"


namespace balsa::zipper {


auto stl2zipper(auto const &M) {
    return eigen::as_zipper(eigen::stl2eigen(M)).eval();
}

}// namespace balsa::zipper
