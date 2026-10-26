#if !defined(BALSA_GEOMETRY_GET_ANGLE_HPP)
#define BALSA_GEOMETRY_GET_ANGLE_HPP
#include <cmath>
#include <numbers>

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
