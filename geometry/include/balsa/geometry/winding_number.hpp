#if !defined(BALSA_GEOMETRY_WINDING_NUMBER_HPP)
#define BALSA_GEOMETRY_WINDING_NUMBER_HPP
#include <cmath>
#include <cstddef>
#include <map>
#include <numbers>

#include "balsa/geometry/get_angle.hpp"
#include "balsa/zipper/concepts/shape_types.hpp"

#include <zipper/Vector.hpp>
#include <zipper/concepts/Matrix.hpp>
#include <zipper/concepts/Vector.hpp>

namespace balsa::geometry {

// ════════════════════════════════════════════════════════════════════════════
// Internal helpers
// ════════════════════════════════════════════════════════════════════════════
namespace internal::winding_number {

    // ── Zipper ─────────────────────────────────────────────────────────────

    // angle formed between A - P to B - P (zipper)
    template<::zipper::concepts::Vector AType, ::zipper::concepts::Vector BType, ::zipper::concepts::Vector PType>
        requires(AType::extents_type::static_extent(0) == 2)
                && (BType::extents_type::static_extent(0) == 2)
                && (PType::extents_type::static_extent(0) == 2)
    auto three_point_angle(
      const AType &A,
      const BType &B,
      const PType &P) -> typename AType::value_type {
        using S = typename AType::value_type;
        ::zipper::Vector<S, 2> a = A - P;
        ::zipper::Vector<S, 2> b = B - P;

        S aa = std::atan2(a(1), a(0));
        S ba = std::atan2(b(1), b(0));
        return get_symmetric_clamped_angle(ba - aa);
    }

    template<zipper::concepts::ColVecs2Compatible VDerived, std::forward_iterator BeginIt, std::forward_iterator EndIt, typename PType>
    auto winding_number_iterator(
      const VDerived &V,
      const BeginIt &beginit,
      const EndIt &endit,
      const PType &p) -> typename VDerived::value_type {
        using S = typename VDerived::value_type;
        S value = 0;
        auto it = beginit;
        for (; it != endit; ++it) {
            auto it1 = it;
            it1++;
            if (it1 == endit) {
                it1 = beginit;
            }

            auto a = V.col(std::size_t(*it));
            auto b = V.col(std::size_t(*it1));

            auto v = three_point_angle(a, b, p);

            value += v;
        }
        value *= 0.5 * std::numbers::inv_pi_v<S>;
        return value;
    }

    template<zipper::concepts::ColVecs2Compatible VDerived, typename BeginIt, typename EndIt, ::zipper::concepts::Vector PType>
        requires(PType::extents_type::static_extent(0) == 2)
    bool interior_winding_number_iterator(const VDerived &V,
                                          const BeginIt &beginit,
                                          const EndIt &endit,
                                          const PType &p) {
        auto v = winding_number_iterator(V, beginit, endit, p);
        return std::abs(v) > .5;
    }

}// namespace internal::winding_number

// ════════════════════════════════════════════════════════════════════════════
// Public API — Zipper overloads
// ════════════════════════════════════════════════════════════════════════════

template<zipper::concepts::ColVecs2Compatible VDerived, typename Container, ::zipper::concepts::Vector PType>
    requires(PType::extents_type::static_extent(0) == 2)
auto winding_number(const VDerived &V,
                    const Container &C,
                    const PType &p) -> typename VDerived::value_type {
    return internal::winding_number::winding_number_iterator(V, C.begin(), C.end(), p);
}

template<zipper::concepts::ColVecs2Compatible VDerived, ::zipper::concepts::Vector PType>
    requires(PType::extents_type::static_extent(0) == 2)
auto winding_number(const VDerived &V,
                    const std::initializer_list<int> &C,
                    const PType &p) -> typename VDerived::value_type {
    return internal::winding_number::winding_number_iterator(V, C.begin(), C.end(), p);
}

template<zipper::concepts::ColVecs2Compatible VDerived, typename Container, ::zipper::concepts::Vector PType>
    requires(PType::extents_type::static_extent(0) == 2)
bool interior_winding_number(const VDerived &V,
                             const Container &C,
                             const PType &p) {
    return internal::winding_number::interior_winding_number_iterator(V, std::begin(C), std::end(C), p);
}

template<zipper::concepts::ColVecs2Compatible VDerived, zipper::concepts::ColVecs2Compatible EDerived, ::zipper::concepts::Vector PType>
    requires(PType::extents_type::static_extent(0) == 2)
auto mesh_winding_number(const VDerived &V,
                         const EDerived &E,
                         const PType &p) -> typename VDerived::value_type {
    using S = typename VDerived::value_type;
    S value = 0;
    for (std::size_t j = 0; j < E.extent(1); ++j) {
        auto e = E.col(j);
        auto a = V.col(std::size_t(e(0)));
        auto b = V.col(std::size_t(e(1)));

        value += internal::winding_number::three_point_angle(a, b, p);
    }
    value *= .5 * std::numbers::inv_pi_v<S>;
    return value;
}

template<zipper::concepts::ColVecs2Compatible VDerived, zipper::concepts::ColVecs2Compatible EDerived, ::zipper::concepts::Vector PType>
    requires(PType::extents_type::static_extent(0) == 2)
auto mesh_winding_number(const VDerived &V,
                         const EDerived &E,
                         const std::map<int, bool> &boundary_map,
                         const PType &p) -> typename VDerived::value_type {
    using S = typename VDerived::value_type;
    S value = 0;
    for (auto &&[idx, sgn] : boundary_map) {
        auto e = E.col(std::size_t(idx));
        std::size_t ai, bi;
        if (sgn) {
            ai = std::size_t(e(0));
            bi = std::size_t(e(1));
        } else {
            ai = std::size_t(e(1));
            bi = std::size_t(e(0));
        }
        auto a = V.col(ai);
        auto b = V.col(bi);

        value += internal::winding_number::three_point_angle(a, b, p);
    }
    value *= .5 * std::numbers::inv_pi_v<S>;
    return value;
}

template<zipper::concepts::ColVecs2Compatible VDerived, zipper::concepts::ColVecs2Compatible EDerived, ::zipper::concepts::Vector PType>
    requires(PType::extents_type::static_extent(0) == 2)
bool interior_mesh_winding_number(const VDerived &V,
                                  const EDerived &E,
                                  const std::map<int, bool> &boundary_map,
                                  const PType &p) {
    auto v = mesh_winding_number(V, E, boundary_map, p);
    return std::abs(v) > .5;
}

template<zipper::concepts::ColVecs2Compatible VDerived, zipper::concepts::ColVecs2Compatible EDerived, ::zipper::concepts::Vector PType>
    requires(PType::extents_type::static_extent(0) == 2)
bool interior_mesh_winding_number(const VDerived &V,
                                  const EDerived &E,
                                  const PType &p) {
    auto v = mesh_winding_number(V, E, p);
    return std::abs(v) > .5;
}

}// namespace balsa::geometry
#endif
