#if !defined(BALSA_GEOMETRY_POINT_CLOUD_SMALLEST_ENCLOSING_SPHERE_WELZL_H)
#define BALSA_GEOMETRY_POINT_CLOUD_SMALLEST_ENCLOSING_SPHERE_WELZL_H
#include <spdlog/spdlog.h>

#include "balsa/geometry/simplex/point_in_circumcircle.hpp"
#include "balsa/zipper/types.hpp"
#include <array>
#include <set>
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
template<::zipper::concepts::MatrixBaseDerived ColVecs>
auto smallest_enclosing_sphere_welzl(const ColVecs &P) -> ::zipper::Vector<typename ColVecs::value_type, ::zipper::utils::extents::plus(ColVecs::static_extent(0), 1)>;


// implementations

namespace detail {

    struct WelzlNode {
        ::zipper::index_type index;
        WelzlNode *parent = nullptr;
        size_t size() const {
            if (parent == nullptr) {
                return 1;
            } else {
                return parent->size() + 1;
            }
        }
        std::vector<::zipper::index_type> as_vector() const {
            return as_vector_with_size(size());
        }
        std::vector<::zipper::index_type> as_vector_with_size(size_t size) const {
            std::vector<::zipper::index_type> ret;
            ret.reserve(size);
            for (const WelzlNode *node = this; node != nullptr; node = node->parent) {
                ret.emplace_back(node->index);
            }
            return ret;
        }
        // assumes that we have already sized things
        template<size_t Size>
        std::array<::zipper::index_type, Size> as_array() const {
            std::array<::zipper::index_type, Size> ret;
            size_t index = 0;
            for (const WelzlNode *node = this; node != nullptr; node = node->parent) {
                ret[index++] = node->index;
            }
            return ret;
        }
    };
    template<::zipper::concepts::MatrixBaseDerived ColVecs>
        requires(ColVecs::static_extent(0) != std::dynamic_extent)
    auto welzl_square_radius(const ColVecs &V, ::zipper::index_type start_index, WelzlNode *R) -> ::zipper::Vector<typename ColVecs::value_type, ::zipper::utils::extents::plus(ColVecs::static_extent(0), 1)> {

        constexpr static ::zipper::index_type Dim = ColVecs::extents_type::static_extent(0);
        using value_type = typename ColVecs::value_type;
        using RetType = ::zipper::Vector<value_type, ::zipper::utils::extents::plus(Dim, 1)>;
        if (start_index == V.cols() || (R != nullptr && R->size() == Dim + 1)) {
            if (R == nullptr) {

                return RetType(::zipper::views::nullary::ConstantView<value_type>(value_type(0)));
            } else {
                // TODO: has to handle more degenerate cases
                size_t size = R->size();

                std::tuple<::zipper::Vector<value_type, Dim>, double> tupret;
                if (size == Dim + 1) {
                    auto indices = R->as_array<Dim + 1>();
                    tupret = simplex::circumcenter_with_squared_radius(V(::zipper::full_extent_t{}, indices));
                } else {
                    auto indices = R->as_vector_with_size(size);
                    tupret = simplex::circumcenter_with_squared_radius(V(::zipper::full_extent_t{}, indices));
                }
                const auto &[C, r] = tupret;
                RetType ret;
                // RetType ret(V.rows() + 1);
                ret.template head<Dim>() = C;
                ret(Dim) = r;
                return ret;
            }
        } else {
            auto CR = welzl_square_radius(V, start_index + 1, R);

            // is p in CR
            {
                auto p = V.col(start_index);
                if ((CR.template head<Dim>() - p).template norm_powered<2>() < CR(Dim)) {
                    return CR;
                }
            }

            WelzlNode node{ start_index, R };
            return welzl_square_radius(V, start_index + 1, &node);
        }
    }
    // template<eigen::concepts::RowStaticCompatible ColVecs>
    // auto welzl_square_radius(const ColVecs &P) -> eigen::ColVectors<typename ColVecs::value_type, ColVecs::RowsAtCompileTime + 1> {

    //    using namespace simplex;
    //    using value_type = typename ColVecs::value_type;
    //    constexpr static int Dim = ColVecs::RowsAtCompileTime;
    //    using RetType = typename eigen::Vector<value_type, Dim + 1>;

    //    RetType ret;
    //    auto center = ret.template head<Dim>();
    //    // need to sqrt this before returning
    //    value_type &square_radius = ret(Dim);

    //    std::array<int, Dim + 1> indices;
    //    std::iota(indices.begin(), indices.end(), 0);

    //    auto update_center = [&]() {
    //        auto V = P(zipper::full_extent_t{}, indices);
    //        // std::cout << "Updating with pts \n"
    //        //           << V << std::endl;
    //        std::tie(center, square_radius) = circumcenter_with_squared_radius(V);
    //        // spdlog::info("Updated circle from indices {}: center {} r2 {}", fmt::join(indices, ","), fmt::join(center, ","), square_radius);
    //    };

    //    update_center();

    //    if (P.cols() < Dim) {
    //        square_radius = std::sqrt(square_radius);
    //        throw std::invalid_argument("Need more points than dimension to define a center");
    //    }


    //    auto inside_circle = [&](const auto &p) -> bool {
    //        return (p - center).squaredNorm() < square_radius;
    //    };

    //    for (int pt_index = Dim + 1; pt_index < P.cols(); ++pt_index) {
    //        if (!inside_circle(P.col(pt_index))) {
    //            for (size_t index_index = 0; index_index < indices.size(); ++index_index) {
    //                int index = indices[index_index];
    //                const int old_index = index;
    //                value_type cur_radius = square_radius;

    //                index = pt_index;
    //                update_center();
    //                if (inside_circle(P.col(old_index))) {

    //                    if (cur_radius < square_radius) {
    //                        index = old_index;
    //                        update_center();
    //                    } else {
    //                        break;
    //                    }
    //                }
    //            }
    //        }
    //    }


    //    return ret;
    //}
}// namespace detail
template<::zipper::concepts::MatrixBaseDerived ColVecs>
auto smallest_enclosing_sphere_welzl(const ColVecs &P) -> ::zipper::Vector<typename ColVecs::value_type, ::zipper::utils::extents::plus(ColVecs::static_extent(0), 1)> {

    std::vector<::zipper::index_type> reorder(P.cols());
    std::ranges::iota(reorder.begin(), reorder.end(), 0);

    std::random_device rd;
    std::mt19937 gen{ rd() };
    std::ranges::shuffle(reorder, gen);

    std::set<int> r;
    auto ret = detail::welzl_square_radius(P(::zipper::full_extent_t{}, reorder), 0, nullptr);
    // auto& rad = ret(Eigen::last-1);
    auto &rad = ret(ret.size() - 1);
    rad = std::sqrt(rad);
    return ret;
}

}// namespace balsa::geometry::point_cloud

#endif

