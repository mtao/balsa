#include <catch2/catch_all.hpp>
#include <iostream>
#include <balsa/eigen/eigen2span.hpp>
#include <range/v3/view/zip.hpp>
#include <balsa/eigen/types.hpp>
TEST_CASE("eigen2span", "[span,eigen]") {


    balsa::eigen::VecXd A(4);
    balsa::eigen::Vec4d B;
    A << 0, 1, 2, 3;
    B << 4, 5, 6, 7;
    auto a = balsa::eigen::eigen2span(A);
    auto b = balsa::eigen::eigen2span(B);
    const auto C = B;
    auto c = balsa::eigen::eigen2span(C);

    REQUIRE(a.size() == 4);
    REQUIRE(b.size() == 4);

    for (const auto &[A, B, a, b, c] : ranges::views::zip(A, B, a, b, c)) {
        CHECK(A == a);
        CHECK(B == b);
        CHECK(b == a + 4);
        CHECK(c == b);
    }
}

