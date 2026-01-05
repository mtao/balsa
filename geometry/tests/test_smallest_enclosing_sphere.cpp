#include <catch2/catch_all.hpp>
#include <zipper/utils/maxCoeff.hpp>
#include <spdlog/stopwatch.h>
#include <optional>
#include <iostream>
#include <fmt/format.h>
#include "circumcenter_utils.hpp"

#include <balsa/geometry/point_cloud/smallest_enclosing_sphere_welzl.hpp>
#include <zipper/views/nullary/RandomView.hpp>
#include <balsa/tensor_types.hpp>


using namespace balsa;
namespace {
template<typename Circ, ::zipper::concepts::MatrixBaseDerived ColVecs>
    requires(!ColVecs::extents_traits::is_dynamic_extent(0))
bool valid_circle_rad2(const Circ &circle, const ColVecs &P) {

    // constexpr static int Dim = ColVecs::RowsAtCompileTime;
    auto [C, r2] = circle;
    // auto C = circle.template head<Dim>();
    // auto r = circle(Dim);

    // auto rads = (P.colwise() - C).colwise().squaredNorm();
    r2 *= 1 + 1e-5;

    auto m = P;
    for (index_type j = 0; j < m.cols(); ++j) {
        auto v = m.col(j);
        v = v - C;
    }
    auto rads = m.colwise().template norm_powered<2>().eval();
    const bool res = (rads.as_array() < (r2)).all();
    return res;
}
template<::zipper::concepts::MatrixBaseDerived ColVecs>
    requires(!ColVecs::extents_traits::is_dynamic_extent(0))
auto brute_force_smallest_circle(const ColVecs &P) -> Vector<typename ColVecs::value_type, ColVecs::extents_type::static_extent(0) + 1> {

    using namespace balsa;
    using namespace balsa::geometry::simplex;
    using value_type = typename ColVecs::value_type;
    constexpr static index_type Dim = ColVecs::static_extent(0);
    using RetType = Vector<value_type, Dim + 1>;

    RetType ret;
    // need to sqrt this before returning
    auto center = ret.template head<Dim>();
    value_type &square_radius = ret(Dim);
    square_radius = std::numeric_limits<double>::max();


    auto try_update = [&](const auto &opret) {
        const auto &[c, r2] = opret;
        if (r2 - 1e-10 <= square_radius) {
            valid_circle_rad2(opret, P);
            bool valid = valid_circle_rad2(opret, P);
            if (valid) {
                center = c;
                square_radius = r2;
            }
        }
    };
    if (P.cols() >= 1) {
        //    center = P.col(0);
        //    square_radius = 1e-10;
        //} else {
        for (index_type i = 0; i < P.cols(); ++i) {
            for (index_type j = 0; j < i; ++j) {
                std::array inds{ i, j };
                auto s0 = P(::zipper::full_extent_t{}, inds);
                using T = std::decay_t<decltype(s0)>;

                static_assert(decltype(s0)::extents_type::static_extent(0) == Dim);
                static_assert(decltype(s0)::extents_type::static_extent(1) == 2);

                auto opret = circumcenter_with_squared_radius(s0);
                // TODO: fix value categories to remove eval
                check_circumcenter_squared(s0.eval(), opret, 1e-4);

                //{
                //    auto [C, R2] = opret;
                //    spdlog::info("{}: {} {}", inds, s0, C);
                //    spdlog::info("{} {} {}", R2, (s0.col(0) - C).template norm_powered<2>(), (s0.col(1) - C).template norm_powered<2>());
                //    spdlog::info("{} {} {}", R2, (s0.col(0) - C), (s0.col(1) - C));
                //}

                // spdlog::info("{}", inds);
                try_update(opret);
                if (Dim >= 2) {
                    for (index_type k = 0; k < j; ++k) {
                        std::array inds{ i, j, k };
                        auto s1 = P(::zipper::full_extent_t{}, inds);
                        static_assert(decltype(s1)::extents_type::static_extent(0) == Dim);
                        static_assert(decltype(s1)::extents_type::static_extent(1) == 3);
                        auto opret = circumcenter_with_squared_radius(s1);
                // TODO: fix value categories to remove eval
                        check_circumcenter_squared(s1.eval(), opret, 1e-4);
                        //{
                        //    auto [C, R2] = opret;
                        //    // spdlog::info("{}", s1);
                        //    // spdlog::info("{} {}: {} {} {}", fmt::join(inds, ","), R2, (s1.col(0) - C).template norm_powered<2>(), (s1.col(1) - C).template norm_powered<2>(), (s1.col(2) - C).template norm_powered<2>());
                        //}

                        // spdlog::info("{}", inds);
                        try_update(opret);
                        if (P.rows() >= 3) {

                            for (index_type l = 0; l <k; ++l) {
                                std::array inds{ i, j, k, l };
                                // spdlog::info("{}", inds);
                                auto s2 = P(::zipper::full_extent_t{}, inds);
                                static_assert(decltype(s2)::extents_type::static_extent(0) == Dim);
                                static_assert(decltype(s2)::extents_type::static_extent(1) == 4);
                                auto opret = circumcenter_with_squared_radius(s2);
                // TODO: fix value categories to remove eval
                                check_circumcenter_squared(s2.eval(), opret, 1e-4);
                                try_update(opret);
                            }
                        }
                    }
                }
            }
        }
    }

    REQUIRE(square_radius < std::numeric_limits<double>::max());
    REQUIRE(valid_circle_rad2(std::make_tuple(center.eval(), square_radius), P));
    square_radius = std::sqrt(square_radius);

    // spdlog::info("Square rad: {}", square_radius);
    return ret;
}

template<index_type D>
std::array<balsa::zipper::ColVectors<double, D>, 2>
  make_inside_outside_points(size_t count, const balsa::zipper::Vector<double, D> &C, double inside, double outside = 0) {
    // make sure inside is
    inside = std::abs(inside);
    if (outside == 0) {
        outside = 5 * inside;
    }
    if (inside >= outside) {
        throw std::invalid_argument(fmt::format("Invalid valid torus: {} to {}", inside, outside));
    }
    double inside_r = inside;


    balsa::zipper::VecXd phi(count);
    phi = ::zipper::views::nullary::uniform_random_view<double>(phi.extents());
    // convert from [-1,1] to [-pi,pi]
    phi = M_PI * phi;


    balsa::zipper::VecXd inner_radii(count);
    inner_radii = ::zipper::views::nullary::uniform_random_view<double>(inner_radii.extents());
    // convert from [-1,1] to [0,pi]
    inner_radii = ::zipper::as_vector((inner_radii.as_array() * inside).abs());

    balsa::VecXd outer_radii(count);
    outer_radii = ::zipper::views::nullary::uniform_random_view<double>(outer_radii.extents());
    // convert from [-1,1] to [0,pi]
    outer_radii = ::zipper::as_vector((outer_radii.as_array() * (outside - inside)).abs() + inside);

    balsa::zipper::VecXd cp = phi.unary_expr([](double x) { return std::cos(x); });
    balsa::zipper::VecXd sp = phi.unary_expr([](double x) { return std::sin(x); });
    {
        balsa::zipper::ColVectors<double, D> inside(count);
        balsa::zipper::ColVectors<double, D> outside(count);

        if constexpr (D == 2) {
            inside.row(0) = cp;
            inside.row(1) = sp;
        } else {
            static_assert(D == 3);
            balsa::zipper::VecXd theta(count);
            theta = ::zipper::views::nullary::uniform_random_view<double>(theta.extents());
            // convert from [-1,1] to [0,pi]
            theta = ::zipper::as_vector((M_PI * theta.as_array()).abs());

            balsa::zipper::VecXd ct = theta.unary_expr([](double x) { return std::cos(x); });
            balsa::zipper::VecXd st = theta.unary_expr([](double x) { return std::sin(x); });

            inside.row(0) = ::zipper::as_vector((sp.as_array() * ct.as_array()));
            inside.row(1) = ::zipper::as_vector((sp.as_array() * st.as_array()));
            inside.row(2) = ::zipper::as_vector(cp.as_array());
        }

        outside = inside;
        // spdlog::info("Norms: {}", outside.colwise().norm());
        // spdlog::info("Inside {}", inner_radii);
        // spdlog::info("Outer {}", outer_radii);
        // spdlog::info("Inside {}", ::zipper::as_vector(inner_radii.as_array() >= inside_r).template cast<int>());
        // spdlog::info("Outer {}", ::zipper::as_vector(outer_radii.as_array() >= inside_r).template cast<int>());

        for (index_type j = 0; j < inside.cols(); ++j) {
            auto v = inside.col(j);
            v = v * inner_radii(j);
            v = v + C;
        }
        for (index_type j = 0; j < outside.cols(); ++j) {
            auto v = outside.col(j);
            v = v * outer_radii(j);
            v = v + C;
        }
        // inside = inside * inner_radii.asDiagonal();
        // outside = outside * outer_radii.asDiagonal();

        // inside.colwise() += C;
        // outside.colwise() += C;
        fmt::print("{}\n{}\n", inside, outside);

        REQUIRE(valid_circle_rad2(std::make_tuple(C, inside_r * inside_r), inside));
        REQUIRE_FALSE(valid_circle_rad2(std::make_tuple(C, inside_r * inside_r), outside));
        // spdlog::info("Making data where center {} rad {} is valid", C, inside_r);
        return { { inside, outside } };
    }
}


}// namespace


TEST_CASE("sphere_sampling_test", "[testing_internal]") {

    auto check = [](const auto &pts, const auto &C, double min, double max) {
        // auto V = (pts.colwise() - C).colwise().norm().eval();
        auto m = pts;
        for (index_type j = 0; j < m.cols(); ++j) {
            auto v = m.col(j);
            v = v - C;
        }
        auto V = m.colwise().template norm_powered<2>().eval();
        CHECK((V.as_array() >= min * min).all());
        CHECK_FALSE(valid_circle_rad2(std::make_tuple(C, min * min), pts));
        if (max != 0) {
            CHECK((V.as_array() <= max * max).all());
            CHECK(valid_circle_rad2(std::make_tuple(C, max * max), pts));
        }
    };

    auto make_check = [&](const balsa::zipper::Vec3d &center, double inner, double outer) {
        auto [inside, outside] = make_inside_outside_points(100, center, inner, outer);
        check(inside, center, 0, inner);
        check(outside, center, inner, outer);
    };
    balsa::zipper::Vec3d center;
    center = ::zipper::views::nullary::ConstantView<double>(0);

    make_check(center, 1, 2);
    make_check(center, 1, 0);
    make_check(center, 3, 5);
    make_check(center, 3, 5);
    center = ::zipper::views::nullary::uniform_random_infinite_view<double>({});
    make_check(center, 1, 2);
    make_check(center, 1, 0);
    make_check(center, 3, 5);
    make_check(center, 3, 5);
}


TEST_CASE("brute_force_test", "[geometry,point_cloud]") {

    auto run = []<int N>(std::integral_constant<int, N>) {
        balsa::zipper::Vector<double, N> center;
        center = ::zipper::views::nullary::uniform_random_infinite_view<double>();

        double init_bound = 3;
        auto [inside, outside] = make_inside_outside_points(10, center, init_bound);

        auto C = brute_force_smallest_circle(inside);

        REQUIRE(C.rows() == N + 1);

        balsa::Vector<double, N> c = C.template head<N>();
        double r = C(N);

        REQUIRE(valid_circle_rad2(std::make_tuple(c, r * r), inside));

        REQUIRE(r <= init_bound);

        {
            auto m = inside;
            for (::zipper::index_type j = 0; j < m.cols(); ++j) {
                auto v = m.col(j);
                v = v - c;
            }
            auto V = m.colwise().norm().eval();
            // spdlog::info("inside rads: {}", V / r);
            // spdlog::info("inside rads: {}", V / (r * r));
            CHECK((V.as_array() <= (r + 1e-8)).all());
        }
        //{
        //    // auto V = (outside.colwise() - c).colwise().norm().eval();
        //    auto m = outside;
        //    for (index_type j = 0; j < m.cols(); ++j) {
        //        auto v = m.col(j);
        //        v = v - c;
        //    }
        //    auto V = m.colwise().norm().eval();
        //    spdlog::info("outside rads: {}", V / r);
        //    CHECK((V.as_array() >= (r - 1e-5)).all());
        //}
    };

    for (int j = 0; j < 5; ++j) {
        run(std::integral_constant<int, 2>{});
        run(std::integral_constant<int, 3>{});
    }
}

TEST_CASE("welzl_base_cases", "[geometry,point_cloud]") {

    auto test = []<int N>(std::integral_constant<int, N>) {
        balsa::zipper::ColVectors<float, N> V(N, N + 5);
        // balsa::zipper::ColVectors<float, N> V(N, 10 * N);
        V = ::zipper::views::nullary::uniform_random_view<double>(V.extents());


        auto circle = balsa::geometry::point_cloud::smallest_enclosing_sphere_welzl(V);
        auto bf_circle = brute_force_smallest_circle(V);
        // circle = brute_force_smallest_circle(V);


        // circle = bf_circle;

        auto C = circle.template head<N>();
        auto r = circle(N);


        auto m = V;
        for (index_type j = 0; j < m.cols(); ++j) {
            auto v = m.col(j);
            v = v - C;
        }
        auto Ds = ::zipper::as_vector(m.colwise().norm().as_array() - r).eval();
        //spdlog::info("Circle {}, brute force circle {}, distances {}", circle, bf_circle, Ds);

        CHECK(::zipper::utils::maxCoeff(Ds) >= 0);
    };

    // TODO: write uniform sample in ball in point_cloud


    test(std::integral_constant<int, 2>{});
    test(std::integral_constant<int, 3>{});
    test(std::integral_constant<int, 4>{});
}


/*
TEST_CASE("welzl", "[geometry,point_cloud]") {

    auto test = []<int N>(std::integral_constant<int, N>, std::optional<int> sample_count = {}) {
        int count = sample_count.has_value() ? sample_count.value() : N + 10;
        balsa::zipper::ColVectors<float, N> V(N, count);
        // balsa::zipper::ColVectors<float, N> V(N, 10 * N);
        V = ::zipper::views::nullary::uniform_random_view<double>(V.extents());

        spdlog::info("Timing check on dim {}", N);

        balsa::zipper::Vector<float, N + 1> circle;
        {
            spdlog::stopwatch sw;
            circle = balsa::geometry::point_cloud::smallest_enclosing_sphere_welzl(V);
            spdlog::info("Welzl {:03} pts elapsed: {}", count, sw);
        }
        // circle = brute_force_smallest_circle(V);


        // circle = bf_circle;

        auto C = circle.template head<N>();
        auto r = circle(N);


        // auto Ds = ((V.colwise() - C).colwise().norm().as_array() - r).eval();
        auto m = V;
        for (::zipper::index_type j = 0; j < m.cols(); ++j) {
            auto v = m.col(j);
            v = v - C;
        }
        auto Ds = (m.colwise().norm().as_array() - r).eval();

        if (!sample_count.has_value()) {
            balsa::zipper::Vector<float, N + 1> bf_circle;
            {
                spdlog::stopwatch sw;
                bf_circle = brute_force_smallest_circle(V);
                spdlog::info("brute force {:03} pts elapsed: {}", count, sw);
            }
            CHECK((bf_circle - circle).norm() < 1e-5);
        }


        CHECK(::zipper::utils::maxCoeff(Ds) >= 0);
    };

    // TODO: write uniform sample in ball in point_cloud


    test(std::integral_constant<int, 2>{});
    test(std::integral_constant<int, 3>{});

    for (int j = 1; j < 4; ++j) {
        test(std::integral_constant<int, 2>{}, 1 << j);
    }
    std::cout << "Three dims" << std::endl;
    for (int j = 1; j < 4; ++j) {
        test(std::integral_constant<int, 3>{}, 1 << j);
    }
    std::cout << "Four dims" << std::endl;
    for (int j = 1; j < 4; ++j) {
        test(std::integral_constant<int, 4>{}, 1 << j);
    }
}
*/
