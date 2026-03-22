#include <catch2/catch_all.hpp>
#include <spdlog/spdlog.h>

#include <iostream>
#include <balsa/geometry/triangle_mesh/volumes.hpp>
#include <zipper/utils/format.hpp>
#include <zipper/utils/min_coeff.hpp>

#include <balsa/geometry/triangle_mesh/earclipping.hpp>
TEST_CASE("earclipping_orientation", "[earclipping]") {
    balsa::zipper::ColVectors<float, 2> V(2, 5);
    std::vector loop{ 0, 1, 2, 3, 4 };


    // 3---------2
    //  \        |
    //   \       |
    //    4      |
    //   /       |
    //  /        |
    // 0---------1
    //
    V.col(0) = balsa::zipper::Vec2f({ 0., 0. });
    V.col(1) = balsa::zipper::Vec2f({ 1., 0. });
    V.col(2) = balsa::zipper::Vec2f({ 1., 1. });
    V.col(3) = balsa::zipper::Vec2f({ 0., 1. });
    V.col(4) = balsa::zipper::Vec2f({ .5f, .5f });


    auto T = balsa::geometry::triangle_mesh::earclipping(V, loop);
    // spdlog::info("{}", T);  // fmt/spdlog version mismatch prevents formatting zipper types

    // 0 1 2
    // 0 2 3
    // 2 3 4
    // 3---------2
    //  \        |
    //   \       |
    //    4      |
    //   /       |
    //  /        |
    // 0---------1

    auto vols = balsa::geometry::triangle_mesh::volumes(V, T);

    // spdlog::info("volumes: {}", vols);  // fmt/spdlog version mismatch prevents formatting zipper types

    CHECK(zipper::utils::minCoeff(vols) > 0);
    loop = { 4, 3, 2, 1, 0 };
    T = balsa::geometry::triangle_mesh::earclipping(V, loop);

    vols = balsa::geometry::triangle_mesh::volumes(V, T);
    CHECK(zipper::utils::minCoeff(vols) > 0);
}
