#if !defined(BALSA_GEOMETRY_TRIANGLE_MESH_EARCLIPPING_HPP)
#define BALSA_GEOMETRY_TRIANGLE_MESH_EARCLIPPING_HPP
#include <list>

#include <range/v3/view/subrange.hpp>
#include <range/v3/view/sliding.hpp>
#include <range/v3/algorithm/copy.hpp>
#include <range/v3/view/take_exactly.hpp>
#include <range/v3/view/cycle.hpp>
#include <range/v3/range/conversion.hpp>
#include "balsa/eigen/stl2eigen.hpp"
#include <Eigen/Core>
#include "balsa/geometry/winding_number.hpp"
#include "balsa/geometry/get_angle.hpp"
#include "balsa/eigen/types.hpp"
#include "balsa/eigen/shape_checks.hpp"
#include <numbers>
#include <utility>

namespace balsa::geometry::triangle_mesh {
template<eigen::concepts::Vec2Compatible VDerived, std::forward_iterator BeginIt, std::forward_iterator EndIt>
requires std::is_integral_v<std::decay_t<decltype(*std::declval<BeginIt>())>>
  std::vector<std::array<std::decay_t<decltype(*std::declval<BeginIt>())>, 3>> earclipping_stl(
    const VDerived &V,
    const BeginIt &beginit,
    const EndIt &endit) {
    using Index = std::decay_t<decltype(*std::declval<BeginIt>())>;
    using Face = std::array<int, 3>;
    std::vector<Face> stlF;

    auto range = ranges::subrange(beginit, endit);


    if constexpr (std::is_integral_v<std::decay_t<decltype(*beginit)>>) {

        size_t size = std::distance(beginit, endit);
        stlF.reserve(size - 2);
        double inner_ang_sum = 0;
        double outer_ang_sum = 0;
        auto triplets = range | ranges::views::cycle// pretend range is cyclic
                        | ranges::views::sliding(3)// take sequences of triplets
                        | ranges::views::take_exactly(std::distance(beginit, endit));// we only want to see each cycle once
        for (auto &&triplet : triplets) {
            Face f;
            ranges::copy(triplet, f.begin());
            const auto &[ai, bi, ci] = f;
            auto a = V.col(ai);
            auto b = V.col(bi);
            auto c = V.col(ci);
            double ang = get_positive_clamped_angle(get_angle(c - b, a - b));
            inner_ang_sum += ang;
            outer_ang_sum += 2 * std::numbers::pi_v<double> - ang;
        }

        bool reverse_orientation = outer_ang_sum < inner_ang_sum;
        if (reverse_orientation) {
        }

        std::list<Index> CL(beginit, endit);

        auto is_earclip = [&](const Face &f) -> bool {
            auto a = V.col(f[0]);
            auto b = V.col(f[1]);
            auto c = V.col(f[2]);
            auto cb = c - b;
            auto ab = a - b;
            // balsa::Vec2d n(-ac.y(),ac.x());
            /*
            if(cb.x() * ab.y() -  cb.y() * ab.x() < 1e-12) {
                return false;
            }
            */
            if (cb.x() * ab.y() - cb.y() * ab.x() < 1e-10) {
                return false;
            }
            // double ang = balsa::geometry::trigonometry::angle(cb,ab)(0);
            double ang = get_positive_clamped_angle(get_angle(c - b, a - b));
            if (ang > std::numbers::pi_v<double> || ang < 0) {
                return false;
            }

            for (auto &&i : range) {
                if (i == f[0] || i == f[1] || i == f[2]) {
                    continue;
                }
                auto v = V.col(i);
                if (interior_winding_number(V, f, v)) {
                    return false;
                }
            }
            return true;
        };

        while (CL.size() > 3) {
            bool earclipped = false;

            for (auto it = CL.begin(); it != CL.end(); ++it) {
                auto it1 = it;
                it1++;
                if (it1 == CL.end()) {
                    it1 = CL.begin();
                }
                auto it2 = it1;
                it2++;
                if (it2 == CL.end()) {
                    it2 = CL.begin();
                }
                Face f{ { *it, *it1, *it2 } };
                if (reverse_orientation) {
                    std::swap(f[0], f[2]);
                }
                if (is_earclip(f)) {
                    stlF.push_back(f);
                    CL.erase(it1);
                    earclipped = true;
                    break;
                }
            }
            if (!earclipped) {
                // logging::warn() << "Earclipping failed!";
                auto it = CL.begin();
                auto it1 = it;
                it1++;
                auto it2 = it1;
                it2++;
                Face f{ { *it, *it1, *it2 } };


                stlF.push_back(f);
                CL.erase(it1);
            }
        }

        Face f;
        std::copy(CL.begin(), CL.end(), f.begin());
        auto a = V.col(f[0]);
        auto b = V.col(f[1]);
        auto c = V.col(f[2]);
        auto cb = c - b;
        auto ab = a - b;
        // auto ac = a - c;
        if (cb.x() * ab.y() - cb.y() * ab.x() < 1e-10) {
            std::swap(f[0], f[1]);
        }
        stlF.push_back(f);
        //} else {
        //    size_t size = 0;
        //    for (auto &&c : ranges::subrange(beginit, endit)) {
        //        size += c.size() - 2;
        //    }
        //    stlF.reserve(size);

        //    for (auto &&c : ranges::subrange(beginit, endit)) {
        //        if (c.size() >= 3) {
        //            auto F = earclipping_stl(V, c.begin(), c.end());
        //            stlF.insert(stlF.end(), F.begin(), F.end());
        //        }
        //    }
    }
    return stlF;
}
// both return balsa::eigen::ColVectors<integral_type, 3>
template<eigen::concepts::ColVecs2Compatible VDerived, typename Container>

auto earclipping(const VDerived &V,
                 const Container &C) {
    return eigen::stl2eigen(earclipping_stl(V, C.begin(), C.end()));
}
template<eigen::concepts::ColVecs2Compatible VDerived, typename T>
auto earclipping(const VDerived &V,
                 const std::initializer_list<T> &C) {
    return eigen::stl2eigen(earclipping_stl(V, C.begin(), C.end()));
}
}// namespace balsa::geometry::triangle_mesh
#endif
