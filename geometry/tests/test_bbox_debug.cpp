#include <zipper/Vector.hpp>
#include <zipper/as.hpp>
#include <zipper/expression/nullary/Constant.hpp>
#include <iostream>

int main() {
    // Test 1: Basic vector creation and comparison
    zipper::Vector<float, 3> a({ 0, 0, 0 });
    zipper::Vector<float, 3> b({ 0, 0, 0 });

    std::cout << "a = {" << a(0) << ", " << a(1) << ", " << a(2) << "}" << std::endl;
    std::cout << "b = {" << b(0) << ", " << b(1) << ", " << b(2) << "}" << std::endl;

    // Test direct coefficient comparison
    std::cout << "a(0)==b(0): " << (a(0) == b(0)) << std::endl;
    std::cout << "a(1)==b(1): " << (a(1) == b(1)) << std::endl;
    std::cout << "a(2)==b(2): " << (a(2) == b(2)) << std::endl;

    // Test as_array
    auto aa = a.as_array();
    auto ba = b.as_array();
    std::cout << "aa(0)=" << aa(0) << " ba(0)=" << ba(0) << std::endl;

    // Test operator== on arrays
    auto eq = (aa == ba);
    std::cout << "eq type size: " << sizeof(eq) << std::endl;
    std::cout << "eq(0)=" << eq.view()(0) << std::endl;
    std::cout << "eq(1)=" << eq.view()(1) << std::endl;
    std::cout << "eq(2)=" << eq.view()(2) << std::endl;

    // Test all
    bool result = eq.all();
    std::cout << "all: " << result << std::endl;

    // Test operator== on vectors directly
    bool vec_eq = (a == b);
    std::cout << "a == b: " << vec_eq << std::endl;

    // Test 2: ConstantView initialization (like BoundingBox default)
    using limits = std::numeric_limits<float>;
    zipper::Vector<float, 3> v_max(zipper::expression::nullary::Constant<float>(limits::max()));
    std::cout << "v_max = {" << v_max(0) << ", " << v_max(1) << ", " << v_max(2) << "}" << std::endl;

    zipper::Vector<float, 3> v_low(zipper::expression::nullary::Constant<float>(limits::lowest()));
    std::cout << "v_low = {" << v_low(0) << ", " << v_low(1) << ", " << v_low(2) << "}" << std::endl;

    // Test 3: min operation
    zipper::Vector<float, 3> x({ 1, 2, 3 });
    auto x_arr = x.as_array();
    auto vmax_arr = v_max.as_array();
    auto min_result = zipper::min(x_arr, vmax_arr);

    // Materialize the min result
    zipper::Vector<float, 3> min_vec;
    // Try assigning through as_vector
    min_vec = zipper::as_vector(min_result);
    std::cout << "min(x, max) = {" << min_vec(0) << ", " << min_vec(1) << ", " << min_vec(2) << "}" << std::endl;

    // Test 4: The actual bounding_box pattern
    zipper::Vector<float, 3> m_min(zipper::expression::nullary::Constant<float>(limits::max()));
    zipper::Vector<float, 3> m_max(zipper::expression::nullary::Constant<float>(limits::lowest()));

    zipper::Vector<float, 3> point({ 0, 0, 0 });

    // This is what expand() does:
    std::cout << "Before expand:" << std::endl;
    std::cout << "m_min = {" << m_min(0) << ", " << m_min(1) << ", " << m_min(2) << "}" << std::endl;
    std::cout << "m_max = {" << m_max(0) << ", " << m_max(1) << ", " << m_max(2) << "}" << std::endl;

    m_min = ::zipper::as_vector(::zipper::min(point.as_array(), m_min.as_array()));
    m_max = ::zipper::as_vector(::zipper::max(point.as_array(), m_max.as_array()));

    std::cout << "After expand:" << std::endl;
    std::cout << "m_min = {" << m_min(0) << ", " << m_min(1) << ", " << m_min(2) << "}" << std::endl;
    std::cout << "m_max = {" << m_max(0) << ", " << m_max(1) << ", " << m_max(2) << "}" << std::endl;

    // Test equality after expand
    zipper::Vector<float, 3> zero({ 0, 0, 0 });
    std::cout << "m_min == zero: " << (m_min == zero) << std::endl;
    std::cout << "m_max == zero: " << (m_max == zero) << std::endl;

    return 0;
}
