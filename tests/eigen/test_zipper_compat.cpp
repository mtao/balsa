#include <catch2/catch_all.hpp>
#include <iostream>
#include <balsa/eigen/zipper_compat.hpp>
#include <range/v3/view/zip.hpp>
#include <balsa/eigen/types.hpp>
TEST_CASE("eigen2zipper", "[zipper,eigen]") {


    balsa::eigen::VecXd A(4);
    balsa::eigen::Vec4d B;
    A << 0, 1, 2, 3;
    B << 4, 5, 6, 7;
    auto a = balsa::eigen::as_zipper(A);
    auto b = balsa::eigen::as_zipper(B);
    const auto C = B;
    auto c = balsa::eigen::as_zipper(C);

    REQUIRE(a.extents() == zipper::dextents<1>(4));
    REQUIRE(b.extents() == zipper::dextents<1>(4));
    REQUIRE(c.extents() == zipper::dextents<1>(4));

    for (const auto &[A, B, a, b, c] : ranges::views::zip(A, B, a.view(), b.view(), c.view())) {
        CHECK(A == a);
        CHECK(B == b);
        CHECK(b == a + 4);
        CHECK(c == b);
    }
}

