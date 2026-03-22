#if !defined(BALSA_GEOMETRY_POINT_CLOUD_SMALLEST_ENCLOSING_SPHERE_WELZL_H)
#define BALSA_GEOMETRY_POINT_CLOUD_SMALLEST_ENCLOSING_SPHERE_WELZL_H

#include "balsa/geometry/simplex/circumcenter.hpp"
#include "balsa/zipper/types.hpp"
#include <zipper/expression/nullary/Constant.hpp>
#include <zipper/utils/extents/extent_arithmetic.hpp>
#include <array>
#include <random>
#include <algorithm>
#include <numeric>
#include <vector>

namespace balsa::geometry::point_cloud {

// Declarations

// returns a D+1 dimensional vector where the first D entries are the center point, the last entry is the radius
template<::zipper::concepts::Matrix ColVecs>
auto smallest_enclosing_sphere_welzl(const ColVecs &P) -> ::zipper::Vector<typename ColVecs::value_type, ::zipper::utils::extents::plus(ColVecs::static_extent(0), 1)>;


// implementations

namespace detail {

    // linked list
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
    template<::zipper::concepts::Matrix ColVecs>
        requires(ColVecs::static_extent(0) != std::dynamic_extent)
    auto welzl_square_radius(const ColVecs &V, ::zipper::index_type start_index, WelzlNode *R) -> ::zipper::Vector<typename ColVecs::value_type, ::zipper::utils::extents::plus(ColVecs::static_extent(0), 1)> {

        constexpr static ::zipper::index_type Dim = ColVecs::extents_type::static_extent(0);
        using value_type = typename ColVecs::value_type;
        using RetType = ::zipper::Vector<value_type, ::zipper::utils::extents::plus(Dim, 1)>;

        // either we have gotten to the end of the extents or the linked list length is the right size for checking diameter
        if (start_index == V.cols() || (R != nullptr && R->size() == Dim + 1)) {
            if (R == nullptr) {// we're at the end but haven't found any points. zero it is
                return RetType(::zipper::expression::nullary::Constant(value_type(0)));
            } else {
                // TODO: has to handle more degenerate cases
                size_t size = R->size();

                // get the best sphere thusfar
                std::tuple<::zipper::Vector<value_type, Dim>, double> tupret{};
                // stupid optimization to not do heap allocation if necessary for getting vertex indices
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
                ::zipper::VectorBase p = V.col(start_index);
                if ((CR.template head<Dim>() - p).template norm_powered<2>() <= CR(Dim)) {
                    return CR;
                }
            }

            WelzlNode node{ start_index, R };
            return welzl_square_radius(V, start_index + 1, &node);
        }
    }
}// namespace detail
template<::zipper::concepts::Matrix ColVecs>
auto smallest_enclosing_sphere_welzl(const ColVecs &P) -> ::zipper::Vector<typename ColVecs::value_type, ::zipper::utils::extents::plus(ColVecs::static_extent(0), 1)> {

    std::vector<::zipper::index_type> reorder(P.cols());
    std::ranges::iota(reorder.begin(), reorder.end(), 0);

    std::random_device rd;
    std::mt19937 gen{ rd() };
    std::ranges::shuffle(reorder, gen);

    auto ret = detail::welzl_square_radius(P(::zipper::full_extent_t{}, reorder), 0, nullptr);
    auto &rad = ret(ret.size() - 1);
    rad = std::sqrt(rad);
    return ret;
}

}// namespace balsa::geometry::point_cloud

#endif
