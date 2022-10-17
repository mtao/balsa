#include <catch2/catch_all.hpp>
#include <iostream>
#include <balsa/geometry/triangle_mesh/volumes.hpp>

#include <balsa/geometry/triangle_mesh/earclipping.hpp>
TEST_CASE("earclipping_orientation", "[earclipping]") {
    balsa::eigen::ColVectors<float, 2> V(2, 5);
    std::vector loop{ 0, 1, 2, 3, 4 };


    // 3---------2
    //  \        |
    //   \       |
    //    4      |
    //   /       |
    //  /        |
    // 0---------1
    //
    V.col(0) = balsa::eigen::Vec2f(0., 0.);
    V.col(1) = balsa::eigen::Vec2f(1., 0.);
    V.col(2) = balsa::eigen::Vec2f(1., 1.);
    V.col(3) = balsa::eigen::Vec2f(0., 1.);
    V.col(4) = balsa::eigen::Vec2f(.5f, .5f);


    auto T = balsa::geometry::triangle_mesh::earclipping(V, loop);
    std::cout << T << std::endl;

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

    std::cout << "Vols: " << vols.transpose() << std::endl;

    CHECK(vols.minCoeff() > 0);
    loop = { 4, 3, 2, 1, 0 };
    T = balsa::geometry::triangle_mesh::earclipping(V, loop);
    std::cout << T << std::endl;

    vols = balsa::geometry::triangle_mesh::volumes(V, T);
    std::cout << "Vols: " << vols.transpose() << std::endl;
    CHECK(vols.minCoeff() > 0);
}
