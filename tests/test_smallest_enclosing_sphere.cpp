#include <catch2/catch.hpp>
#include <iostream>

#include <balsa/geometry/point_cloud/smallest_enclosing_sphere_welzl.hpp>


namespace {
template<typename Circ, balsa::eigen::concepts::RowStaticCompatible ColVecs>
bool valid_circle(const Circ &circle, const ColVecs &P) {

    // constexpr static int Dim = ColVecs::RowsAtCompileTime;
    auto [C, r] = circle;
    // auto C = circle.template head<Dim>();
    // auto r = circle(Dim);

    auto Ds = ((P.colwise() - C).colwise().norm().array() - r).eval();

    return Ds.maxCoeff() < 1e-5;
}
}// namespace
template<balsa::eigen::concepts::RowStaticCompatible ColVecs>
auto brute_force_smallest_circle(const ColVecs &P) -> balsa::eigen::Vector<typename ColVecs::Scalar, ColVecs::RowsAtCompileTime + 1> {

    using namespace balsa::geometry::simplex;
    using Scalar = typename ColVecs::Scalar;
    constexpr static int Dim = ColVecs::RowsAtCompileTime;
    using RetType = typename balsa::eigen::Vector<Scalar, Dim + 1>;

    RetType ret;
    // need to sqrt this before returning
    auto center = ret.template head<Dim>();
    Scalar &square_radius = ret(Dim);
    square_radius = std::numeric_limits<double>::max();


    for (int j = 0; j < P.cols(); ++j) {
        for (int k = 1; k < P.cols(); ++k) {
            for (int l = 2; l < P.cols(); ++l) {
                auto opret = circumcenter_with_squared_radius(P(Eigen::all, { j, k, l }));

                bool valid = valid_circle(opret, P);
                if (valid) {
                    if (square_radius > std::get<1>(opret)) {
                        std::cout << std::get<1>(opret) << std::endl;

                        std::tie(center, square_radius) = opret;
                    }
                }
            }
        }
    }
    square_radius = std::sqrt(square_radius);
    return ret;
}


TEST_CASE("welzl", "[geometry,point_cloud]") {

    auto test = []<int N>(std::integral_constant<int, N>) {
        balsa::eigen::ColVectors<float, N> V(N, N + 5);
        // balsa::eigen::ColVectors<float, N> V(N, 10 * N);
        V.setRandom();

        std::cout << "Input points:\n";
        std::cout << V << std::endl;

        auto circle = balsa::geometry::point_cloud::smallest_enclosing_sphere_welzl(V);
        auto bf_circle = brute_force_smallest_circle(V);
        // circle = brute_force_smallest_circle(V);


        // circle = bf_circle;

        auto C = circle.template head<N>();
        auto r = circle(N);


        auto Ds = ((V.colwise() - C).colwise().norm().array() - r).eval();
        std::cout << "circle: " << circle.transpose() << std::endl;
        std::cout << "brute force circle: " << bf_circle.transpose() << std::endl;
        std::cout << "Distances: " << Ds << std::endl;

        CHECK(Ds.maxCoeff() >= 0);
    };

    // TODO: write uniform sample in ball in point_cloud


    test(std::integral_constant<int, 2>{});
}
