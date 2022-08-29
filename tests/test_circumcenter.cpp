#include <catch2/catch.hpp>
#include <iostream>

#include <balsa/geometry/simplex/circumcenter.hpp>

TEST_CASE("circumcenter", "[simplex]") {
    auto random_simplex_test = []<int N>(std::integral_constant<int, N>) {
    balsa::eigen::Matrix<double,N,N+1> V;
    V.setRandom();

    balsa::eigen::Vector<double,N> C = balsa::geometry::simplex::circumcenter(V);

    balsa::eigen::Matrix<double,N,N+1> D = V.colwise() - C;
    balsa::eigen::Vector<double,N+1> R = D.colwise().norm().transpose();

    double mean = R.mean();

    for(int j = 0; j < R.size(); ++j)
    {
        CHECK(R(j) == Approx(mean));
    }
    balsa::eigen::Vector<double,N> CSPD = balsa::geometry::simplex::circumcenter_spd(V);
    balsa::eigen::Vector<double,N> CSPSD = balsa::geometry::simplex::circumcenter_spsd(V);

    for(int j = 0; j < N; ++j)
    {
        CHECK(CSPD(j) == Approx(C(j)));
        CHECK(CSPSD(j) == Approx(C(j)));
    }
    };
    
    for(int j = 0; j < 10; ++j)
    {
        random_simplex_test(std::integral_constant<int, 2>{});
        random_simplex_test(std::integral_constant<int, 3>{});
        random_simplex_test(std::integral_constant<int, 4>{});
        random_simplex_test(std::integral_constant<int, 5>{});
    }


}
