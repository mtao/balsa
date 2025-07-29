#include <catch2/catch_all.hpp>
#include <iostream>

#include <zipper/utils/format.hpp>
#include <zipper/views/nullary/RandomView.hpp>
#include <zipper/utils/meanCoeff.hpp>
#include <balsa/geometry/simplex/circumcenter.hpp>

using namespace Catch;
TEST_CASE("circumcenter", "[simplex]") {
    auto random_simplex_test = []<int N>(std::integral_constant<int, N>) {
        balsa::zipper::Matrix<double, N, N + 1> V = zipper::views::nullary::uniform_random_view<double>({});

        auto [C, radius] = balsa::geometry::simplex::circumcenter_with_radius(V);

        // balsa::zipper::Matrix<double, N, N + 1> D = V.colwise() - C;
        // balsa::zipper::Vector<double, N + 1> R = D.colwise().norm().transpose();
        balsa::zipper::Matrix<double, N, N + 1> D;
        balsa::zipper::Vector<double, N + 1> R;
        for (zipper::index_type j = 0; j < V.extent(1); ++j) {
            auto d = D.col(j);
            d = V.col(j) - C;
            R(j) = d.norm();
        }

        double mean = zipper::utils::meanCoeff(R);
        CHECK(radius == Approx(mean));
        //spdlog::info("points in R^{} got radius {}, distances to center were {}",  N, mean, fmt::join(R, ","));


        for (zipper::index_type j = 0; j < R.view().size(); ++j) {
            CHECK(R(j) == Approx(radius));
        }
        balsa::zipper::Vector<double, N> CSPD = balsa::geometry::simplex::circumcenter_spd(V);
        balsa::zipper::Vector<double, N> CSPSD = balsa::geometry::simplex::circumcenter_spsd(V);

        for (int j = 0; j < N; ++j) {
            CHECK(CSPD(j) == Approx(C(j)));
            CHECK(CSPSD(j) == Approx(C(j)));
        }
    };

    for (int j = 0; j < 10; ++j) {
        random_simplex_test(std::integral_constant<int, 2>{});
        random_simplex_test(std::integral_constant<int, 3>{});
        random_simplex_test(std::integral_constant<int, 4>{});
        random_simplex_test(std::integral_constant<int, 5>{});
    }
}

TEST_CASE("smallest_circle", "[simplex]") {
    auto random_simplex_test = [](zipper::index_type n, zipper::index_type m) {
        balsa::zipper::MatrixX<double> V = zipper::views::nullary::uniform_random_view<double>(zipper::create_dextents(n, m));

        auto [C, radius] = balsa::geometry::simplex::circumcenter_with_radius(V);
        static_assert(std::decay_t<decltype(C)>::extents_type::rank() == 1);
        REQUIRE(C.extent(0) == n);

        // auto D = V.colwise() - C;
        // balsa::zipper::VectorX<double> R = D.colwise().norm().transpose();
        balsa::zipper::Matrix<double, std::dynamic_extent, std::dynamic_extent> D(n, m);
        balsa::zipper::Vector<double, std::dynamic_extent> R(m);
        for (zipper::index_type j = 0; j < V.extent(1); ++j) {
            auto d = D.col(j);
            REQUIRE(d.extent(0) == V.col(j).extent(0));
            REQUIRE(d.extent(0) == C.extent(0));
            d = V.col(j) - C;
            R(j) = d.norm();
        }

        double mean = zipper::utils::meanCoeff(R);
        CHECK(radius == Approx(mean));

        //spdlog::info("{} points in R^{} got radius {}, distances to center were {}", m, n, mean, fmt::join(R, ","));
    };

    for (int j = 0; j < 10; ++j) {
        for (int i = 2; i < 5; ++i) {
            for (int k = 1; k <= i + 1; ++k) {
                random_simplex_test(i, k);
            }
        }
    }
}
