#include <cstdio>
#include <limits>
#include <zipper/Vector.hpp>
#include <zipper/as.hpp>
#include <zipper/ArrayBase.hpp>
#include <zipper/expression/nullary/Constant.hpp>

int main() {
    using limits = std::numeric_limits<float>;

    // Test 1: ConstantView initialization
    zipper::Vector<float, 3> v1 = zipper::expression::nullary::Constant<float>(limits::max());
    printf("v1 (should be max): %g %g %g\n", (double)v1(0), (double)v1(1), (double)v1(2));

    zipper::Vector<float, 3> v2 = zipper::expression::nullary::Constant<float>(limits::lowest());
    printf("v2 (should be lowest): %g %g %g\n", (double)v2(0), (double)v2(1), (double)v2(2));

    // Test 2: Simple equality
    zipper::Vector<float, 3> a({ 0.0f, 0.0f, 0.0f });
    zipper::Vector<float, 3> b({ 0.0f, 0.0f, 0.0f });
    printf("a: %g %g %g\n", (double)a(0), (double)a(1), (double)a(2));
    printf("b: %g %g %g\n", (double)b(0), (double)b(1), (double)b(2));

    auto aa = a.as_array();
    auto ba = b.as_array();
    auto eq = aa == ba;
    printf("eq[0]=%d eq[1]=%d eq[2]=%d\n", (int)eq.view()(0), (int)eq.view()(1), (int)eq.view()(2));
    bool all_eq = eq.all();
    printf("a == b: %d\n", (int)all_eq);

    // Test 3: min/max operations
    zipper::Vector<float, 3> c({ 5.0f, 5.0f, 5.0f });
    auto min_result = zipper::as_vector(zipper::min(a.as_array(), c.as_array()));
    printf("min(a,c): %g %g %g\n", (double)min_result(0), (double)min_result(1), (double)min_result(2));

    // Test 4: assign min result back
    zipper::Vector<float, 3> d = zipper::expression::nullary::Constant<float>(limits::max());
    printf("d before expand: %g %g %g\n", (double)d(0), (double)d(1), (double)d(2));
    d = zipper::as_vector(zipper::min(a.as_array(), d.as_array()));
    printf("d after min with a: %g %g %g\n", (double)d(0), (double)d(1), (double)d(2));

    return 0;
}
