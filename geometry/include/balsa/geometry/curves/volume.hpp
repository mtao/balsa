#if !defined(BALSA_GEOMETRY_CURVES_VOLUME_HPP)
#define BALSA_GEOMETRY_CURVES_VOLUME_HPP

#include <cstddef>
#include <iterator>
#include <stdexcept>

#include "balsa/eigen/types.hpp"
#include "balsa/eigen/concepts/shape_types.hpp"
#include "balsa/eigen/shape_checks.hpp"
#include "balsa/zipper/concepts/shape_types.hpp"

#include <zipper/concepts/Matrix.hpp>

namespace balsa::geometry::curves {

// ════════════════════════════════════════════════════════════════════════════
// Eigen overloads
// ════════════════════════════════════════════════════════════════════════════

// Signed area enclosed by a 2D closed polygon (shoelace formula).
// V is a 2xN matrix of column-vector vertices, traversed in order.
// Positive = counter-clockwise winding, negative = clockwise.
template<typename Derived>
auto curve_volume(const Eigen::MatrixBase<Derived> &V) ->
  typename Derived::Scalar {
    typename Derived::Scalar ret = 0;
    static_assert(Derived::RowsAtCompileTime == 2 || Derived::RowsAtCompileTime == Eigen::Dynamic);
    if (V.rows() != 2) {
        throw std::invalid_argument("curve_volume only works on ColVecs2d-like inputs");
    }
    for (int i = 0; i < V.cols(); ++i) {
        auto a = V.col(i);
        auto b = V.col((i + 1) % V.cols());
        ret += a.x() * b.y() - a.y() * b.x();
    }
    return .5 * ret;
}

// Signed area of a 2D polygon with vertices indexed by [beginit, endit).
template<eigen::concepts::ColVecs2Compatible Derived, std::forward_iterator BeginIt, std::forward_iterator EndIt>
auto curve_volume(const Derived &V, const BeginIt &beginit, const EndIt &endit) -> typename Derived::Scalar {
    auto it = beginit;
    auto it1 = beginit;
    it1++;
    typename Derived::Scalar ret = 0;
    eigen::row_check_with_throw<2>(V);
    for (; it != endit; ++it, ++it1) {
        if (it1 == endit) {
            it1 = beginit;
        }
        auto a = V.col(*it);
        auto b = V.col(*it1);
        ret += a.x() * b.y() - a.y() * b.x();
    }
    return .5 * ret;
}

// Signed area of a 2D polygon with vertices indexed by a container.
template<eigen::concepts::ColVecs2Compatible Derived, typename Container>
auto curve_volume(const Eigen::MatrixBase<Derived> &V, const Container &C) ->
  typename Derived::Scalar {
    return curve_volume(V, C.begin(), C.end());
}

// ════════════════════════════════════════════════════════════════════════════
// Zipper overloads
// ════════════════════════════════════════════════════════════════════════════

// Signed area enclosed by a 2D closed polygon (shoelace formula).
// V is a 2xN zipper matrix of column-vector vertices, traversed in order.
template<zipper::concepts::ColVecs2Compatible VDerived>
auto curve_volume(const VDerived &V) -> typename VDerived::value_type {
    using S = typename VDerived::value_type;
    S ret = 0;
    const auto n = V.extent(1);
    for (std::size_t i = 0; i < n; ++i) {
        auto a = V.col(i);
        auto b = V.col((i + 1) % n);
        ret += a(0) * b(1) - a(1) * b(0);
    }
    return S{ .5 } * ret;
}

// Signed area of a 2D polygon with vertices indexed by [beginit, endit).
template<zipper::concepts::ColVecs2Compatible VDerived, std::forward_iterator BeginIt, std::forward_iterator EndIt>
auto curve_volume(const VDerived &V, const BeginIt &beginit, const EndIt &endit) -> typename VDerived::value_type {
    using S = typename VDerived::value_type;
    auto it = beginit;
    auto it1 = beginit;
    it1++;
    S ret = 0;
    for (; it != endit; ++it, ++it1) {
        if (it1 == endit) {
            it1 = beginit;
        }
        auto a = V.col(std::size_t(*it));
        auto b = V.col(std::size_t(*it1));
        ret += a(0) * b(1) - a(1) * b(0);
    }
    return S{ .5 } * ret;
}

// Signed area of a 2D polygon with vertices indexed by a container.
template<zipper::concepts::ColVecs2Compatible VDerived, typename Container>
auto curve_volume(const VDerived &V, const Container &C) -> typename VDerived::value_type {
    return curve_volume(V, C.begin(), C.end());
}

}// namespace balsa::geometry::curves
#endif
