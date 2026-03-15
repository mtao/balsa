#include <catch2/catch_all.hpp>
#include <zipper/utils/maxCoeff.hpp>
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

    auto [C, r2] = circle;

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

// Recursively enumerate all k-subsets of indices [0, n) and try each as a
// circumscribing simplex. This generalizes the old nested-loop approach so
// it works for any dimension (including D=4 which needs quintuples).
template<::zipper::concepts::MatrixBaseDerived ColVecs, typename TryUpdateFn>
    requires(!ColVecs::extents_traits::is_dynamic_extent(0))
void brute_force_enumerate_subsets(
  const ColVecs &P,
  std::vector<index_type> &current,
  index_type max_index,
  index_type subset_size,
  TryUpdateFn &try_update) {

    using namespace balsa::geometry::simplex;

    if (current.size() == static_cast<size_t>(subset_size)) {
        auto s = P(::zipper::full_extent_t{}, current);
        auto opret = circumcenter_with_squared_radius(s);
        check_circumcenter_squared(s.eval(), opret, 1e-4);
        try_update(opret);
        return;
    }

    // Pick next index strictly less than the last one picked (descending order)
    index_type upper = current.empty() ? max_index : current.back();
    // Need enough remaining indices to fill the subset
    index_type remaining_needed = subset_size - static_cast<index_type>(current.size()) - 1;
    for (index_type i = upper - 1; i >= remaining_needed && i < max_index; --i) {
        current.push_back(i);
        brute_force_enumerate_subsets(P, current, max_index, subset_size, try_update);
        current.pop_back();
    }
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
    auto center = ret.template head<Dim>();
    value_type &square_radius = ret(Dim);
    square_radius = std::numeric_limits<double>::max();


    auto try_update = [&](const auto &opret) {
        const auto &[c, r2] = opret;
        if (r2 - 1e-10 <= square_radius) {
            bool valid = valid_circle_rad2(opret, P);
            if (valid) {
                center = c;
                square_radius = r2;
            }
        }
    };

    // Try all k-subsets for k = 2, 3, ..., min(D+1, P.cols())
    index_type max_subset = std::min<index_type>(Dim + 1, P.cols());
    for (index_type k = 2; k <= max_subset; ++k) {
        std::vector<index_type> current;
        current.reserve(k);
        brute_force_enumerate_subsets(P, current, P.cols(), k, try_update);
    }

    REQUIRE(square_radius < std::numeric_limits<double>::max());
    REQUIRE(valid_circle_rad2(std::make_tuple(center.eval(), square_radius), P));
    square_radius = std::sqrt(square_radius);

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

        // maxCoeff(Ds) should be ≈ 0: the tightest sphere has at least one
        // boundary point. Allow small floating-point slack in both directions.
        CHECK(::zipper::utils::maxCoeff(Ds) >= -1e-5);
        CHECK(::zipper::utils::maxCoeff(Ds) <= 1e-5);
    };

    // TODO: write uniform sample in ball in point_cloud


    test(std::integral_constant<int, 2>{});
    test(std::integral_constant<int, 3>{});
    test(std::integral_constant<int, 4>{});
}
