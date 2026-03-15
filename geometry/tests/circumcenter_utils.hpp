#pragma once
#include <zipper/concepts/MatrixBaseDerived.hpp>
#include <balsa/geometry/simplex/circumcenter.hpp>
#include <catch2/catch_approx.hpp>


namespace {

void check_circumcenter_squared(::zipper::concepts::MatrixBaseDerived auto const &V, const auto &CR, double margin = 1e-8) {

    using namespace Catch;
    auto [C, radius] = CR;
    for (zipper::index_type j = 0; j < V.extent(1); ++j) {
        zipper::Vector d = (V.col(j) - C);
        CHECK(d.template norm_powered<2>() == Approx(radius).margin(margin));
    }
}
}// namespace
