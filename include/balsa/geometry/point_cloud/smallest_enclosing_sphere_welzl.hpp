#if !defined(BALSA_GEOMETRY_POINT_CLOUD_SMALLEST_ENCLOSING_SPHERE_WELZL_H)
#define BALSA_GEOMETRY_POINT_CLOUD_SMALLEST_ENCLOSING_SPHERE_WELZL_H
#include <spdlog/spdlog.h>

#include "balsa/eigen/concepts/shape_types.hpp"
#include "balsa/geometry/simplex/point_in_circumcircle.hpp"
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
template<eigen::concepts::RowStaticCompatible ColVecs>
auto smallest_enclosing_sphere_welzl(const ColVecs &P) -> eigen::Vector<typename ColVecs::Scalar, ColVecs::RowsAtCompileTime + 1>;


// implementations

namespace detail {
    template<eigen::concepts::RowStaticCompatible ColVecs>
    auto welzl(const ColVecs &P) -> eigen::ColVectors<typename ColVecs::Scalar, ColVecs::RowsAtCompileTime + 1> {

        using namespace simplex;
        using Scalar = typename ColVecs::Scalar;
        constexpr static int Dim = ColVecs::RowsAtCompileTime;
        using RetType = typename eigen::Vector<Scalar, Dim + 1>;

        RetType ret;
        auto center = ret.template head<Dim>();
        // need to sqrt this before returning
        Scalar &square_radius = ret(Dim);

        std::array<int, Dim + 1> indices;
        std::iota(indices.begin(), indices.end(), 0);

        auto update_center = [&]() {
            auto V = P(Eigen::all, indices);
            // std::cout << "Updating with pts \n"
            //           << V << std::endl;
            std::tie(center, square_radius) = circumcenter_with_squared_radius(V);
            // spdlog::info("Updated circle from indices {}: center {} r2 {}", fmt::join(indices, ","), fmt::join(center, ","), square_radius);
        };

        update_center();

        if (P.cols() < Dim) {
            square_radius = std::sqrt(square_radius);
            throw std::invalid_argument("Need more points than dimension to define a center");
        }


        auto inside_circle = [&](const auto &p) -> bool {
            return (p - center).squaredNorm() < square_radius;
        };

        for (int pt_index = Dim + 1; pt_index < P.cols(); ++pt_index) {
            if (!inside_circle(P.col(pt_index))) {
                for (size_t index_index = 0; index_index < indices.size(); ++index_index) {
                    int index = indices[index_index];
                    const int old_index = index;
                    Scalar cur_radius = square_radius;

                    index = pt_index;
                    update_center();
                    if (inside_circle(P.col(old_index))) {

                        if (cur_radius < square_radius) {
                            index = old_index;
                            update_center();
                        } else {
                            break;
                        }
                    }
                }
            }
        }


        square_radius = std::sqrt(square_radius);
        return ret;
    }
}// namespace detail
template<eigen::concepts::RowStaticCompatible ColVecs>
auto smallest_enclosing_sphere_welzl(const ColVecs &P) -> eigen::Vector<typename ColVecs::Scalar, ColVecs::RowsAtCompileTime + 1> {

    std::vector<int> reorder(P.cols());
    std::iota(reorder.begin(), reorder.end(), 0);

    std::random_device rd;
    std::mt19937 gen{ rd() };
    std::ranges::shuffle(reorder, gen);

    return detail::welzl(P(Eigen::all, reorder));
}

}// namespace balsa::geometry::point_cloud

#endif

