#include <catch2/catch.hpp>
#include <range/v3/view/iota.hpp>
#include <range/v3/view/repeat.hpp>
#include <range/v3/view/transform.hpp>
#include <range/v3/action/push_back.hpp>
#include <range/v3/range/conversion.hpp>
#include <iostream>
#include <balsa/eigen/stl2eigen.hpp>
TEST_CASE("stl2eigen", "[ranges]") {


    auto a = ranges::views::iota(0, 10) | ranges::views::transform([](int i) {
                 return std::array<int, 2>{ { i, i } };
             })
             | ranges::to<std::vector<std::array<int, 2>>>;


    balsa::eigen::container_size(a);
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

