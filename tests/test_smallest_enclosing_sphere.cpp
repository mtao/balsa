#include <catch2/catch.hpp>
#include <spdlog/stopwatch.h>
#include <optional>
#include <iostream>
#include <fmt/format.h>

#include <balsa/geometry/point_cloud/smallest_enclosing_sphere_welzl.hpp>


namespace {
template<typename Circ, balsa::eigen::concepts::RowStaticCompatible ColVecs>
bool valid_circle_rad2(const Circ &circle, const ColVecs &P) {

    // constexpr static int Dim = ColVecs::RowsAtCompileTime;
    auto [C, r2] = circle;
    // auto C = circle.template head<Dim>();
    // auto r = circle(Dim);

    auto rads = (P.colwise() - C).colwise().squaredNorm();
    return (rads.array() <= r2).all();

}
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


    auto try_update = [&](const auto& opret)
    {

        const auto& [c,r2] = opret;
        if(std::get<1>(opret) < square_radius)
        {
            bool valid = valid_circle_rad2(opret, P);
            if (valid) {
                center = c;
                square_radius = r2;
            }
        }
    };
    if(P.cols() == 1)
    {
        center = P;
        square_radius = 1e-10;
    }
    for (int i = 0; i < P.cols(); ++i) {
        for (int j = 1; j < P.cols(); ++j) {
            std::array inds{i,j};
            auto opret = circumcenter_with_squared_radius(P(Eigen::all, inds));
            try_update(opret);
            for (int k = 2; k < P.cols(); ++k) {
                std::array inds{i,j,k};
                auto opret = circumcenter_with_squared_radius(P(Eigen::all, inds));

                try_update(opret);
                if(P.rows() == 3) {

                    for (int l = 3; l < P.cols(); ++l) {
                        std::array inds{i,j,k,l};
                        auto opret = circumcenter_with_squared_radius(P(Eigen::all, inds));
                        try_update(opret);

                    }
                }
            }
        }
    }
    square_radius = std::sqrt(square_radius);
    return ret;
}

template <int D>
std::array<balsa::eigen::ColVectors<double,D>,2>
make_inside_outside_points(size_t count , const balsa::eigen::Vector<double,D>& C, double inside, double outside=0)
{
    // make sure inside is 
    inside = std::abs(inside);
    if(outside == 0)
    {
        outside = 5 * inside;
    }
    if(inside >= outside)
    {
        throw std::invalid_argument(fmt::format("Invalid valid torus: {} to {}", inside, outside));
    }

    balsa::eigen::VecXd phi(count);
    phi.setRandom();
    // convert from [-1,1] to [-pi,pi]
    phi = phi.array() * M_PI;


    balsa::eigen::VecXd inner_radii(count);
    inner_radii.setRandom();
    // convert from [-1,1] to [0,pi]
    inner_radii = (inner_radii.array() * inside).abs();

    balsa::eigen::VecXd outer_radii(count);
    outer_radii.setRandom();
    // convert from [-1,1] to [0,pi]
    outer_radii = (outer_radii.array() * (outside- inside)).abs() + inside;

    balsa::eigen::VecXd cp = phi.array().cos();
    balsa::eigen::VecXd sp = phi.array().sin();
    {
    balsa::eigen::ColVectors<double,D> inside(D,count);
    balsa::eigen::ColVectors<double,D> outside(D,count);

    if constexpr(D == 2)
    {
        inside.row(0) = cp.array().transpose() ;
        inside.row(1) = sp.array().transpose() ;
    } else {
        balsa::eigen::VecXd theta(count);
        theta.setRandom();
        // convert from [-1,1] to [0,pi]
        theta = (theta.array() * M_PI).abs();

        balsa::eigen::VecXd ct = theta.array().cos();
        balsa::eigen::VecXd st = theta.array().sin();

        inside.row(0) = (sp.array() * ct.array()).transpose();
        inside.row(1) = (sp.array() * st.array()).transpose();
        inside.row(2) = cp.array().transpose();
    }

    outside = inside;

    inside = inside * inner_radii.asDiagonal();
    outside = outside * outer_radii.asDiagonal();

    inside.colwise() += C;
    outside.colwise() += C;

    return {{inside,outside}};
    }

}



}// namespace


TEST_CASE("sphere_sampling_test", "[testing_internal]") {

    auto check = [](const auto& pts, const auto& C, double min, double max)
    {
        auto V = (pts.colwise() - C).colwise().norm().eval();
        CHECK((V.array() >= min).all());
        CHECK_FALSE(valid_circle_rad2(std::make_tuple(C,min*min), pts));
        if(max != 0)
        {
            CHECK((V.array() <= max).all());
            CHECK(valid_circle_rad2(std::make_tuple(C,max*max), pts));
        }


    };

    auto make_check = [&](const balsa::eigen::Vec3d & center, double inner, double outer) {
    auto [inside,outside] = make_inside_outside_points(100,center,inner,outer);
    check(inside,center,0,inner);
    check(outside,center,inner,outer);
    };
    balsa::eigen::Vec3d center;
    center.setZero();

    make_check(center,1,2);
    make_check(center,1,0);
    make_check(center,3,5);
    make_check(center,3,5);
    center.setRandom();
    make_check(center,1,2);
    make_check(center,1,0);
    make_check(center,3,5);
    make_check(center,3,5);

    

}


TEST_CASE("brute_force_test", "[geometry,point_cloud]") {


    auto run = []<int N>(std::integral_constant<int,N>)
    {
    balsa::eigen::Vector<double,N> center;
    center.setRandom();

    double init_bound = 3;
    auto [inside,outside] = make_inside_outside_points(10,center,init_bound);
    auto C = brute_force_smallest_circle(inside);

    REQUIRE(C.rows() == N+1);

    balsa::eigen::Vector<double,N> c = C.template head<N>();
    double r = C(N);

    REQUIRE(r <= init_bound);

    {
    auto V = (inside.colwise() - c).colwise().norm().eval();
    CHECK((V.array() <= r).all());
    }
    {
    auto V = (outside.colwise() - c).colwise().norm().eval();
    CHECK((V.array() >= r).all());
    }
    };

    for(int j = 0; j < 5; ++j)
    {
        run(std::integral_constant<int,2>{});
        run(std::integral_constant<int,3>{});
    }

    

}

//TEST_CASE("welzl_base_cases", "[geometry,point_cloud]") {
//
//    return;
//    auto test = []<int N>(std::integral_constant<int, N>) {
//        balsa::eigen::ColVectors<float, N> V(N, N + 5);
//        // balsa::eigen::ColVectors<float, N> V(N, 10 * N);
//        V.setRandom();
//
//        std::cout << "Input points:\n";
//        std::cout << V << std::endl;
//
//        auto circle = balsa::geometry::point_cloud::smallest_enclosing_sphere_welzl(V);
//        auto bf_circle = brute_force_smallest_circle(V);
//        // circle = brute_force_smallest_circle(V);
//
//
//        // circle = bf_circle;
//
//        auto C = circle.template head<N>();
//        auto r = circle(N);
//
//
//        auto Ds = ((V.colwise() - C).colwise().norm().array() - r).eval();
//        std::cout << "circle: " << circle.transpose() << std::endl;
//        std::cout << "brute force circle: " << bf_circle.transpose() << std::endl;
//        std::cout << "Distances: " << Ds << std::endl;
//
//        CHECK(Ds.maxCoeff() >= 0);
//    };
//
//    // TODO: write uniform sample in ball in point_cloud
//
//
//    test(std::integral_constant<int, 2>{});
//}


TEST_CASE("welzl", "[geometry,point_cloud]") {

    auto test = []<int N>(std::integral_constant<int, N>, std::optional<int> sample_count = {}) {

        int count = sample_count.has_value() ? sample_count.value() : N + 10;
        balsa::eigen::ColVectors<float, N> V(N, count);
        // balsa::eigen::ColVectors<float, N> V(N, 10 * N);
        V.setRandom();


        balsa::eigen::Vector<float,N+1> circle;
        {
            spdlog::stopwatch sw;
            circle = balsa::geometry::point_cloud::smallest_enclosing_sphere_welzl(V);
            spdlog::info("Welzl {:03}pts elapsed: {}", count, sw);
        }
        // circle = brute_force_smallest_circle(V);


        // circle = bf_circle;

        auto C = circle.template head<N>();
        auto r = circle(N);


        auto Ds = ((V.colwise() - C).colwise().norm().array() - r).eval();

        if(!sample_count.has_value())
        {
            balsa::eigen::Vector<float,N+1> bf_circle;
            {
                spdlog::stopwatch sw;
                bf_circle = brute_force_smallest_circle(V);
            spdlog::info("brute force {:03}pts elapsed: {}", count, sw);
            }
            CHECK(bf_circle.isApprox(circle));
        }


        CHECK(Ds.maxCoeff() >= 0);
    };

    // TODO: write uniform sample in ball in point_cloud


    test(std::integral_constant<int, 2>{});
    test(std::integral_constant<int, 3>{});

    for(int j = 1; j < 4; ++j)
    {
    test(std::integral_constant<int, 2>{},1 << j);
    }
    std::cout << "Three dims" << std::endl;
    for(int j = 1; j < 4; ++j)
    {
    test(std::integral_constant<int, 3>{},1 << j);
    }
    std::cout << "Four dims" << std::endl;
    for(int j = 1; j < 4; ++j)
    {
    test(std::integral_constant<int, 4>{},1 << j);
    }
}
