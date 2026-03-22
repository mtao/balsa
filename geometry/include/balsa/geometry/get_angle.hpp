#if !defined(BALSA_GEOMETRY_GET_ANGLE_HPP)
#define BALSA_GEOMETRY_GET_ANGLE_HPP
#include <cmath>
#include <numbers>

#include "balsa/eigen/concepts/shape_types.hpp"

#include <zipper/concepts/Vector.hpp>
#include <zipper/concepts/Matrix.hpp>
#include <zipper/Vector.hpp>

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

// ── Eigen overloads ────────────────────────────────────────────────────

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

    auto a = P.row(0);
    auto b = P.row(1);
    for (int i = 0; i < P.cols(); ++i) {
        A(i) = get_angle_scalar<S>(a(i), b(i));
    }
    return A;
}

// Returns the angle of between each col from A to B clockwise
template<eigen::concepts::ColVecs2Compatible Derived, eigen::concepts::ColVecs2Compatible Derived1>
auto get_angle_from_colvectors(const Derived &A, const Derived1 &B) {
    auto R = (get_angle(B) - get_angle(A)).eval();
    return R;
}

// ── Zipper overloads ───────────────────────────────────────────────────

template<::zipper::concepts::Vector T>
    requires(T::extents_type::static_extent(0) == 2)
auto get_angle(const T &v) -> typename T::value_type {
    return get_angle_scalar(v(0), v(1));
}

template<::zipper::concepts::Vector T, ::zipper::concepts::Vector U>
    requires(T::extents_type::static_extent(0) == 2) && (U::extents_type::static_extent(0) == 2)
auto get_angle(const T &a, const U &b) -> typename T::value_type {
    return get_angle(b) - get_angle(a);
}

}// namespace balsa::geometry
#endif
