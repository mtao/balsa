#include <catch2/catch_all.hpp>
#include <iostream>

#include <balsa/geometry/simplex/circumcenter.hpp>

using namespace Catch;
TEST_CASE("circumcenter", "[simplex]") {
    auto random_simplex_test = []<int N>(std::integral_constant<int, N>) {
        balsa::eigen::Matrix<double, N, N + 1> V;
        V.setRandom();

        auto [C, radius] = balsa::geometry::simplex::circumcenter_with_radius(V);

        balsa::eigen::Matrix<double, N, N + 1> D = V.colwise() - C;
        balsa::eigen::Vector<double, N + 1> R = D.colwise().norm().transpose();

        double mean = R.mean();
        CHECK(radius == Approx(mean));


        for (int j = 0; j < R.size(); ++j) {
            CHECK(R(j) == Approx(radius));
        }
        balsa::eigen::Vector<double, N> CSPD = balsa::geometry::simplex::circumcenter_spd(V);
        balsa::eigen::Vector<double, N> CSPSD = balsa::geometry::simplex::circumcenter_spsd(V);

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
    auto random_simplex_test = [](int n, int m) {
        balsa::eigen::MatrixX<double> V(n,m);
        V.setRandom();

        auto [C, radius] = balsa::geometry::simplex::circumcenter_with_radius(V);

        auto  D = V.colwise() - C;
        balsa::eigen::VectorX<double> R = D.colwise().norm().transpose();

        double mean = R.mean();
        CHECK(radius == Approx(mean));

        //spdlog::info("{} points in R^{} got radius {}, distances to center were {}", m, n, mean, fmt::join(R,","));
    };

    for (int j = 0; j < 10; ++j) {
        for (int i = 2; i < 5; ++i) {
            for(int k = 1; k <= i+1; ++k)
            {
                random_simplex_test(i,k);
            }
        }
    }
}
