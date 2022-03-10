#if !defined(BALSA_VISUALIZATION_SCENE_GRAPH_TRANSFORMATION_TRAITS_HPP)
#define BALSA_VISUALIZATION_SCENE_GRAPH_TRANSFORMATION_TRAITS_HPP

#include <concepts>

namespace balsa::visualization::scene_graph {

namespace concepts {
    template<typename TT>
    concept transformation_traits = requires(TT::projection_type p, TT::transform_type t, TT::point_type, TT::homogeneous_type h) {

        { TT::matrix_product(t, h) } -> std::is_same_as<TT::homogeneous_type>;
            TT::matrix_product(p, TT::matrix_product(t,)
    }
}// namespace concepts


template<int D, typename T = float>
struct matrix_transformation_traits {
    using point_type = glm::vec<D, T>;
    using homogeneous_point_type = glm::vec<D + 1, T>;
    using vector_type = glm::vec<D, T>;
    using projection_type = glm::mat<3, D + 1, T>;
    using transform_type = glm::mat<D + 1, D + 1, T>;


    using as_homogeneous_point = [](const point_type &p) { return homogeneous_point_type(p, T(1)); } using as_homogeneous_vector = [](const vector_type &p) { return homogeneous_vector_type(p, T(0)); }
};
}// namespace balsa::visualization::scene_graph

#endif
