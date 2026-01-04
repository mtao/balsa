#include <catch2/catch_all.hpp>
#include <iostream>
#include <balsa/eigen/zipper_compat.hpp>
#include <range/v3/view/zip.hpp>
#include <balsa/eigen/types.hpp>
#include <zipper/Vector.hpp>
#include <zipper/Matrix.hpp>
TEST_CASE("eigen2zipper", "[zipper,eigen]") {


    balsa::eigen::VecXd A(4);
    balsa::eigen::Vec4d B;
    A << 0, 1, 2, 3;
    B << 4, 5, 6, 7;
    balsa::eigen::Mat2d M;
    M << 9, 10, 11, 12;
    auto a = balsa::eigen::as_zipper(A);
    auto b = balsa::eigen::as_zipper(B);
    const auto C = B;
    auto c = balsa::eigen::as_zipper(C);

    auto m = balsa::eigen::as_zipper(M);

    REQUIRE(a.extents() == zipper::dextents<1>(4));
    REQUIRE(b.extents() == zipper::dextents<1>(4));
    REQUIRE(c.extents() == zipper::dextents<1>(4));

    REQUIRE(m.extents() == zipper::dextents<2>(2, 2));
    for (int j = 0; j < 2; ++j) {
        for (int k = 0; k < 2; ++k) {
            CHECK(m(j, k) == M(j, k));
        }
    }

    for (const auto &[A, B, a, b, c] : ranges::views::zip(A, B, a.view(), b.view(), c.view())) {
        CHECK(A == a);
        CHECK(B == b);
        CHECK(b == a + 4);
        CHECK(c == b);
    }
    auto sum = A + B;
    auto sumz = balsa::eigen::as_zipper(sum);

    REQUIRE(int(sumz.extent(0)) == sum.size());
    for (const auto &[s, S] : ranges::views::zip(sum, sumz.view())) {
        CHECK(s == S);
    }
}
TEST_CASE("zipper2eigen", "[zipper,eigen]") {


    zipper::Vector<double, std::dynamic_extent> A(4);
    zipper::Vector<double, 4> B;
    A = { 0, 1, 2, 3 };
    B = { 4, 5, 6, 7 };
    auto a = balsa::eigen::as_eigen(A);
    auto b = balsa::eigen::as_eigen(B);
    const auto C = B;
    auto c = balsa::eigen::as_eigen(C);

    static_assert(std::decay_t<decltype(a)>::RowsAtCompileTime == Eigen::Dynamic);
    static_assert(std::decay_t<decltype(a)>::ColsAtCompileTime == 1);
    static_assert(std::decay_t<decltype(b)>::RowsAtCompileTime == 4);
    static_assert(std::decay_t<decltype(b)>::ColsAtCompileTime == 1);
    REQUIRE(a.rows() == 4);
    REQUIRE(b.rows() == 4);
    REQUIRE(c.rows() == 4);


    for (const auto &[A, B, a, b, c] : ranges::views::zip(A.view(), B.view(), a, b, c)) {
        CHECK(A == a);
        CHECK(B == b);
        CHECK(b == a + 4);
        CHECK(c == b);
    }
    auto sumz = A + B;
    auto sum = balsa::eigen::as_eigen(sumz);

    REQUIRE(int(sumz.extent(0)) == sum.size());
    for (int i = 0; i < sum.size(); ++i) {
        CHECK(sumz(i) == sum(i));
    }
}

