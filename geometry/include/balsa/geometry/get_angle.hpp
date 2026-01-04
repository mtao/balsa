#if !defined(BALSA_GEOMETRY_GET_ANGLE_HPP)
#define BALSA_GEOMETRY_GET_ANGLE_HPP
#include <numbers>
#include "balsa/eigen/shape_checks.hpp"


namespace balsa::geometry {

// pushes an angle to the range [0,2*std::numbers::pi_v<S>)
template<typename S>
S get_positive_clamped_angle(S ang) {
    while (ang > 2 * std::numbers::pi_v<S>) {
        ang -= 2 * std::numbers::pi_v<S>;
    }
    while (ang < 0) {
        ang += 2 * std::numbers::pi_v<S>;
    }
    return ang;
}
// pushes an angle to the range [-std::numbers::pi_v<S>,std::numbers::pi_v<S>)
template<typename S>
S get_symmetric_clamped_angle(S ang) {
    while (ang > std::numbers::pi_v<S>) {
        ang -= 2 * std::numbers::pi_v<S>;
    }
    while (ang < -std::numbers::pi_v<S>) {
        ang += 2 * std::numbers::pi_v<S>;
    }
    return ang;
}

// Returns the angle between a vector and [1,0]^*
template<typename S>
S get_angle_scalar(S cos, S sin) {
    return std::atan2(sin, cos);
}
template<eigen::concepts::Vec2Compatible T>
typename T::Scalar get_angle(const T &v) {
    return get_angle_scalar(v.x(), v.y());
}

template<eigen::concepts::Vec2Compatible T, eigen::concepts::Vec2Compatible U>
typename T::Scalar get_angle(const T &a, const U &b) {
    return get_angle(b) - get_angle(a);
}


// Returns the angle for each vector in a ColVector and returns a rowvector
template<eigen::concepts::ColVecs2Compatible T>
auto get_angle_from_colvectors(const T &P) {
    using S = typename T::Scalar;
    Eigen::Matrix<S, 1, T::ColsAtCompileTime> A(1, P.cols());

#ifdef TODO_CHECK_WHEN_THIS_IS_OK
    A = P.row(0).binaryExpr(P.row(1), std::function(get_angle_scalar<S>));
#else
    auto a = P.row(0);
    auto b = P.row(1);
    for (int i = 0; i < P.cols(); ++i) {
        A(i) = angle_scalar<S>(a(i), b(i));
    }
#endif
    return A;
}

// Returns the angle of between each col from A to B clockwise
template<eigen::concepts::ColVecs2Compatible Derived, eigen::concepts::ColVecs2Compatible Derived1>
auto get_angle_from_colvectors(const Derived &A, const Derived1 &B) {
    auto R = (get_angle(B) - get_angle(A)).eval();
    return R;
}

}// namespace balsa::geometry
#endif
