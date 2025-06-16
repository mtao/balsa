#include <catch2/catch_all.hpp>
#include <iostream>

#include <balsa/geometry/simplex/volume.hpp>
#include <balsa/zipper/types.hpp>
#include <zipper/views/nullary/ConstantView.hpp>
using namespace Catch;

TEST_CASE("volume", "[simplex]") {
    {
        // template arguments in templates still have some minor limitations but integral_constant helps handle them!
        auto identity_test = []<int N>(std::integral_constant<int, N>) {
            balsa::zipper::ColVectors<float, N> V(N, N + 1);
            V.col(0) = zipper::views::nullary::ConstantView<float>(0);
            V.template slice<zipper::static_index_t<1>, zipper::full_extent_t>() = zipper::views::nullary::ConstantView<float>(0);
            for(int j = 0; j < N; ++j) {
                V(j,j+1) = 1;
            }
            //V.template rightCols<N>().setIdentity();
            double vol = balsa::geometry::simplex::volume(V);
            double svol = balsa::geometry::simplex::volume_signed(V);
            double usvol = balsa::geometry::simplex::volume_unsigned(V);
            CHECK(vol == Approx(1.0 / balsa::utils::factorial(N)));
            CHECK(vol == Approx(svol));
            CHECK(vol == Approx(usvol));


            // we can swap now! but gotta be careful about aliasing
            //V(Eigen::all, { 1, 2 }) = V(Eigen::all, { 2, 1 }).eval();
            //double minus_svol = balsa::geometry::simplex::volume_signed(V);
            //CHECK(-vol == Approx(minus_svol));
        };


        identity_test(std::integral_constant<int, 2>{});
        identity_test(std::integral_constant<int, 3>{});
        identity_test(std::integral_constant<int, 4>{});
        identity_test(std::integral_constant<int, 5>{});
    }
}
