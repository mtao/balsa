#if !defined(BALSA_VISUALIZATION_SCENE_GRAPH_TRANSFORMATION_TRAITS_HPP)
#define BALSA_VISUALIZATION_SCENE_GRAPH_TRANSFORMATION_TRAITS_HPP

#include <concepts>
#include <glm/matrix.hpp>

namespace balsa::visualization::scene_graph {

namespace concepts {
    // A transformation_traits type must provide projection, transform, point,
    // and homogeneous types, plus a matrix_product operation that composes them.
    template<typename TT>
    concept transformation_traits = requires(typename TT::projection_type p, typename TT::transform_type t, typename TT::point_type pt, typename TT::homogeneous_type h) {
        { TT::matrix_product(t, h) } -> std::same_as<typename TT::homogeneous_type>;
        { TT::matrix_product(p, TT::matrix_product(t, h)) };
    };
}// namespace concepts


template<int D, typename T = float>
struct matrix_transformation_traits {
    using point_type = glm::vec<D, T>;
    using homogeneous_point_type = glm::vec<D + 1, T>;
    using vector_type = glm::vec<D, T>;
    using projection_type = glm::mat<3, D + 1, T>;
    using transform_type = glm::mat<D + 1, D + 1, T>;
    using homogeneous_type = glm::vec<D + 1, T>;

    static homogeneous_point_type as_homogeneous_point(const point_type &p) { return homogeneous_point_type(p, T(1)); }
    static homogeneous_type as_homogeneous_vector(const vector_type &v) { return homogeneous_type(v, T(0)); }

    static homogeneous_type matrix_product(const transform_type &m, const homogeneous_type &v) { return m * v; }
    static auto matrix_product(const projection_type &p, const homogeneous_type &v) { return p * v; }
};
}// namespace balsa::visualization::scene_graph

#endif
