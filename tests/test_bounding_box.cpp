#include <catch2/catch_all.hpp>
#include <iostream>

#include <balsa/geometry/bounding_box.hpp>
#include <balsa/zipper/types.hpp>
#include <zipper/views/nullary/ConstantView.hpp>
#include <zipper/views/nullary/RandomView.hpp>

#include <bitset>
using namespace Catch;

TEST_CASE("bounding_box", "[bounding_box][geometry]") {
    {
        balsa::zipper::ColVectors<float, 3> V(3, 5);
        V.col(0) = { 0, 0, 0 };
        V.col(1) = { 0, 0, 0 };
        V.col(2) = { 0, 0, 0 };
        V.col(3) = { 0, 0, 0 };
        V.col(4) = { 0, 0, 0 };

        auto bb = balsa::geometry::bounding_box(V);


        spdlog::info("{} {}", fmt::join(bb.min(), ","), fmt::join(zipper::Vector(V.col(0)), ","));
        spdlog::info("{} {}", fmt::join(bb.max(), ","), fmt::join(zipper::Vector(V.col(0)), ","));
        CHECK(bb.min() == zipper::Vector<float, 3>({ 0, 0, 0 }));
        CHECK(bb.min() == zipper::Vector<float, 3>({ 0, 0, 0 }));
        CHECK(bb.max() == bb.min());
        CHECK(bb.range() == zipper::Vector<float, 3>({ 0, 0, 0 }));


        V.col(1) = { 1, 3, 4 };
        V.col(2) = { -2, -5, 20 };
        bb = balsa::geometry::bounding_box(V);

        CHECK(bb.min() == zipper::Vector<float, 3>({ -2, -5, 0 }));
        CHECK(bb.max() == zipper::Vector<float, 3>({ 1, 3, 20 }));
        CHECK(bb.range() == zipper::Vector<float, 3>({ 3, 8, 20 }));

        CHECK(bb.corner(0) == zipper::Vector<float, 3>({ -2, -5, 0 }));
        CHECK(bb.corner(1) == zipper::Vector<float, 3>({ 1, -5, 0 }));
        CHECK(bb.corner(2) == zipper::Vector<float, 3>({ -2, 3, 0 }));
        CHECK(bb.corner(3) == zipper::Vector<float, 3>({ 1, 3, 0 }));
        CHECK(bb.corner(4) == zipper::Vector<float, 3>({ -2, -5, 20 }));
        CHECK(bb.corner(5) == zipper::Vector<float, 3>({ 1, -5, 20 }));
        CHECK(bb.corner(6) == zipper::Vector<float, 3>({ -2, 3, 20 }));
        CHECK(bb.corner(7) == zipper::Vector<float, 3>({ 1, 3, 20 }));
    }
    {

        balsa::geometry::BoundingBox bb(zipper::Vector<float, 2>({ 0, 1 }));
        balsa::geometry::BoundingBox bb2(zipper::Vector<float, 2>({ 4, 4 }));

        auto bb3 = bb;
        bb3.expand(bb2);

        CHECK(bb3.min() == bb.min());
        CHECK(bb3.max() == bb2.max());
        CHECK(bb3.range().eval() == zipper::Vector<float, 2>({ 4, 3 }));
    }
}
