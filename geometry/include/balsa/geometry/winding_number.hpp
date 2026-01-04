#if !defined(BALSA_GEOMETRY_WINDING_NUMBER_HPP)
#define BALSA_GEOMETRY_WINDING_NUMBER_HPP
#include <iostream>
#include <numbers>

#include "balsa/eigen/types.hpp"
#include "balsa/eigen/shape_checks.hpp"
#include "balsa/geometry/get_angle.hpp"
//#define USE_DET_COTAN
namespace balsa::geometry {
namespace internal::winding_number {
    // angle formed between A - P to B - P
    template<eigen::concepts::Vec2Compatible PDerived, eigen::concepts::Vec2Compatible ADerived, eigen::concepts::Vec2Compatible BDerived>
    typename ADerived::Scalar three_point_angle(
      const ADerived &A,
      const BDerived &B,
      const PDerived &P) {
        using S = typename ADerived::Scalar;
#ifdef USE_DET_COTAN
        auto mytan = [](auto &&a, auto &&b) -> S {
            return (a.x() * b.y() - a.y() * b.x()) / (a.dot(b));
        };
#endif
        auto a = (A - P).eval();
        auto b = (B - P).eval();

#ifdef USE_DET_COTAN
        if (std::abs(a.dot(b)) < 1e-5) {
            continue;
        }
        S ang = mytan(a, b);

#else
        S aa = std::atan2(a.y(), a.x());
        S ba = std::atan2(b.y(), b.x());
        S ang = ba - aa;
#endif
        return ang;
    }


    template<eigen::concepts::ColVecs2Compatible PDerived, eigen::concepts::ColVecs2Compatible VDerived, std::forward_iterator BeginIt, std::forward_iterator EndIt>
    typename VDerived::Scalar winding_number_iterator(
      const VDerived &V,
      const BeginIt &beginit,
      const EndIt &endit,
      const PDerived &p) {
        using S = typename VDerived::Scalar;
        S value = 0;
        auto it = beginit;
        for (; it != endit; ++it) {
            auto it1 = it;
            it1++;
            if (it1 == endit) {
                it1 = beginit;
            }

            auto a = V.col(*it);
            auto b = V.col(*it1);

            auto v = three_point_angle(a, b, p);

            value += v;
        }
        value *= 0.5 * std::numbers::inv_pi_v<S>;
        return value;
    }

    template<eigen::concepts::Vec2Compatible PDerived, eigen::concepts::Vec2Compatible VDerived, typename BeginIt, typename EndIt>
    bool interior_winding_number_iterator(const VDerived &V,
                                          const BeginIt &beginit,
                                          const EndIt &endit,
                                          const Eigen::MatrixBase<PDerived> &p) {
        auto v = winding_number_iterator(V, beginit, endit, p);
        return std::abs(v) > .5;
    }
}// namespace internal::winding_number

template<eigen::concepts::Vec2Compatible PDerived, eigen::concepts::Vec2Compatible VDerived, typename Container>
typename VDerived::Scalar winding_number(const VDerived &V,
                                         const Container &C,
                                         const Eigen::MatrixBase<PDerived> &p) {
    return internal::winding_number::winding_number_iterator(V, C.begin(), C.end(), p);
}

template<eigen::concepts::Vec2Compatible PDerived, eigen::concepts::Vec2Compatible VDerived>
typename VDerived::Scalar winding_number(const VDerived &V,
                                         const std::initializer_list<int> &C,
                                         const Eigen::MatrixBase<PDerived> &p) {
    return internal::winding_number::winding_number_iterator(V, C.begin(), C.end(), p);
}


template<eigen::concepts::Vec2Compatible PDerived, eigen::concepts::Vec2Compatible VDerived, typename Container>
bool interior_winding_number(const VDerived &V,
                             const Container &C,
                             const PDerived &p) {
    return internal::winding_number::interior_winding_number_iterator(V, std::begin(C), std::end(C), p);
}
//template<eigen::concepts::Vec2Compatible PDerived, eigen::concepts::Vec2Compatible VDerived>
//bool interior_winding_number_iterator(const VDerived &V,
//                                      const std::initializer_list<int> &C,
//                                      const Eigen::MatrixBase<PDerived> &p) {
//    return internal::winding_number::interior_winding_number(V, std::begin(C), std::end(C), p);
//}

template<eigen::concepts::Vec2Compatible PDerived, eigen::concepts::Vec2Compatible VDerived, eigen::concepts::Vec2Compatible EDerived>
auto mesh_winding_number(const VDerived &V,
                         const EDerived &E,
                         const Eigen::MatrixBase<PDerived> &p) {
    using S = typename VDerived::Scalar;
    if constexpr (PDerived::ColsAtCompileTime == 1) {
        S value = 0;
        for (int j = 0; j < E.cols(); ++j) {
            auto e = E.col(j);
            auto a = V.col(e(0));
            auto b = V.col(e(1));

            value += internal::winding_number::three_point_angle(a, b, p);
        }
        value *= .5 * std::numbers::inv_pi_v<S>;
        return value;
    } else {
        balsa::eigen::Vector<S, PDerived::ColsAtCompileTime> R(p.cols());
        for (int j = 0; j < p.cols(); ++j) {
            R(j) = mesh_winding_number(V, E, p.col(j));
        }

        return R;
    }
}

template<eigen::concepts::Vec2Compatible PDerived, eigen::concepts::Vec2Compatible VDerived, eigen::concepts::Vec2Compatible EDerived>
double mesh_winding_number(const VDerived &V,
                           const EDerived &E,
                           const std::map<int, bool> &boundary_map,
                           const Eigen::MatrixBase<PDerived> &p) {
    using S = typename VDerived::Scalar;
    S value = 0;
    for (auto &&[idx, sgn] : boundary_map) {
        auto e = E.col(idx);
        int ai, bi;
        if (sgn) {
            ai = e(0);
            bi = e(1);
        } else {
            ai = e(1);
            bi = e(0);
        }
        auto a = V.col(ai);
        auto b = V.col(bi);

        value += internal::winding_number::three_point_angle(a, b, p);
        // std::cout << "{" << value << "}";
    }
    value *= .5 * std::numbers::inv_pi_v<S>;
    return value;
}
template<eigen::concepts::Vec2Compatible PDerived, eigen::concepts::Vec2Compatible VDerived, eigen::concepts::Vec2Compatible EDerived>
bool interior_mesh_winding_number(const VDerived &V,
                                  const EDerived &E,
                                  const std::map<int, bool> &boundary_map,
                                  const Eigen::MatrixBase<PDerived> &p) {
    auto v = mesh_winding_number(V, E, boundary_map, p);
    return std::abs(v) > .5;
}

template<eigen::concepts::Vec2Compatible PDerived, eigen::concepts::Vec2Compatible VDerived, eigen::concepts::Vec2Compatible EDerived>
auto interior_mesh_winding_number(const VDerived &V,
                                  const EDerived &E,
                                  const Eigen::MatrixBase<PDerived> &p) {
    if constexpr (PDerived::ColsAtCompileTime == 1) {
        auto v = mesh_winding_number(V, E, p);
        return std::abs(v) > .5;
    } else {
        return (mesh_winding_number(V, E, p).array().abs() > .5).eval();
    }
}
}// namespace balsa::geometry
#endif
