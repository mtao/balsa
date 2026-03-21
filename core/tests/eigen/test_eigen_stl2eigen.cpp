#include <catch2/catch_all.hpp>
#include <ranges>
#include <iostream>
#include <balsa/eigen/stl2eigen.hpp>
TEST_CASE("stl2eigen", "[ranges]") {


    auto a = std::views::iota(0, 10) | std::views::transform([](int i) {
                 return std::array<int, 2>{ { i, i } };
             })
             | std::ranges::to<std::vector<std::array<int, 2>>>();


    REQUIRE(balsa::eigen::container_size(a) == Eigen::Dynamic);
    auto b = balsa::eigen::stl2eigen(a);
    std::cout << b << std::endl;

    REQUIRE(b.rows() == 2);
    REQUIRE(size_t(b.cols()) == a.size());

    for (int j = 0; j < b.rows(); ++j) {
        for (int k = 0; k < b.cols(); ++k) {
            CHECK(a[k][j] == b(j, k));
        }
    }
}
