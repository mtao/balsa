#include <catch2/catch.hpp>
#include <iostream>

#include <balsa/geometry/simplex/volume.hpp>

TEST_CASE("volume", "[simplex]") {
    {
        // template arguments in templates still have some minor limitations but integral_constant helps handle them!
        auto identity_test = []<int N>(std::integral_constant<int, N>) {
            balsa::eigen::ColVectors<float, N> V(N, N + 1);
            V.col(0).setZero();
            V.template rightCols<N>().setIdentity();
            double vol = balsa::geometry::simplex::volume(V);
            double svol = balsa::geometry::simplex::volume_signed(V);
            double usvol = balsa::geometry::simplex::volume_unsigned(V);
            CHECK(vol == Approx(1.0 / balsa::utils::factorial(N)));
            CHECK(vol == Approx(svol));
            CHECK(vol == Approx(usvol));


            // we can swap now! but gotta be careful about aliasing
            V(Eigen::all, { 1, 2 }) = V(Eigen::all, { 2, 1 }).eval();
            double minus_svol = balsa::geometry::simplex::volume_signed(V);
            CHECK(-vol == Approx(minus_svol));
        };


        identity_test(std::integral_constant<int, 2>{});
        identity_test(std::integral_constant<int, 3>{});
        identity_test(std::integral_constant<int, 4>{});
        identity_test(std::integral_constant<int, 5>{});
    }
}
