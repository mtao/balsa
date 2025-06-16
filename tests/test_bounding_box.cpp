#include <catch2/catch_all.hpp>
#include <iostream>

#include <balsa/geometry/bounding_box.hpp>
#include <balsa/zipper/types.hpp>
#include <zipper/views/nullary/ConstantView.hpp>
#include <zipper/views/nullary/RandomView.hpp>
using namespace Catch;

TEST_CASE("bounding_box", "[bounding_box][geometry]") {
    {
            balsa::zipper::ColVectors<float, 3> V(3, 5);
            V.col(0) = {0,0,0};
            V.col(1) = {0,0,0};
            V.col(2) = {0,0,0};
            V.col(3) = {0,0,0};
            V.col(4) = {0,0,0};

            auto bb = balsa::geometry::bounding_box(V);


            CHECK(bb.min() == V.row(0));
            CHECK(bb.max() == V.row(0));


    }
}
