#if !defined(BALSA_GEOMETRY_POINT_CLOUD_SMALLEST_ENCLOSING_SPHERE_WELZL_H)
#define BALSA_GEOMETRY_POINT_CLOUD_SMALLEST_ENCLOSING_SPHERE_WELZL_H

#include "balsa/eigen/concepts/shape_types.hpp"
#include "balsa/eigen/types.hpp"
#include <array>
#include <random>
#include <algorithm>
#include <numeric>
#include <vector>

namespace balsa::geometry::simplex {

auto circumcenter(auto &&P) {
    return P.col(0);
}
}// namespace balsa::geometry::simplex
namespace balsa::geometry::point_cloud {

// Declarations

// returns a D+1 dimensional vector where the first D entries are the center point, the last entry is the radius
template<eigen::concepts::ColStaticCompatible ColVecsType>
auto smallest_enclosing_sphere_welzl(const ColVecsType &P) -> eigen::ColVectors<typename ColVecsType::Scalar, ColVecsType::RowsAtCompileTime + 1>;


// implementations

namespace detail {
    template<eigen::concepts::ColStaticCompatible ColVecsType>
    auto welzl(const ColVecsType &P) -> eigen::ColVectors<typename ColVecsType::Scalar, ColVecsType::RowsAtCompileTime + 1> {

        using Scalar = typename ColVecsType::Scalar;
        constexpr static int Dim = ColVecsType::RowsAtCompileTime;
        using RetType = typename eigen::ColVectors<Scalar, Dim>;

        RetType ret;
        auto center = ret.template head<Dim>();
        Scalar &radius = ret(Dim);

        std::array<int, Dim + 1> indices;
        std::iota(indices.begin(), indices.end(), 0);

        if (P.cols() < Dim) {
            center = simplex::circumcenter(P);
            radius = (center - P.col(0)).norm();
            throw std::invalid_argument("Need more points than dimension to define a center");
        }


        auto update_center = [&]() {
            center = simplex::circumcenter(P(Eigen::all, indices));
            radius = (center - P.col(indices[0])).norm();
        };

        auto inside_circle = [&](const auto &p) -> bool {
            return (p - center).squaredNorm() < radius * radius;
        };

        for (int pt_idx = Dim; pt_idx < P.cols(); ++pt_idx) {
            if (!inside_circle(P.col(pt_idx))) {
                for (auto &swap_pt_idx : indices) {
                    Scalar cur_radius = radius;
                    std::swap(pt_idx, swap_pt_idx);
                    update_center();
                    if (cur_radius < radius) {
                        std::swap(pt_idx, swap_pt_idx);
                        update_center();
                    }
                }
            }
        }


        return ret;
    }
}// namespace detail
template<eigen::concepts::ColStaticCompatible ColVecsType>
auto smallest_enclosing_sphere_welzl(const ColVecsType &P) -> eigen::ColVectors<typename ColVecsType::Scalar, ColVecsType::RowsAtCompileTime + 1> {

    std::vector<int> reorder(P.cols());
    std::iota(reorder.begin(), reorder.end(), 0);

    std::random_device rd;
    std::mt19937 gen{ rd() };
    std::ranges::shuffle(reorder, gen);

    return detail::welzl(P(Eigen::all, reorder));
}

}// namespace balsa::geometry::point_cloud

#endif

