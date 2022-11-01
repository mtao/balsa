#if !defined(BALSA_VISUALIZATION_SCENE_GRAPH_TRANSFORMATION_TRAITS_HPP)
#define BALSA_VISUALIZATION_SCENE_GRAPH_TRANSFORMATION_TRAITS_HPP

#include <concepts>
#include <glm/glm.hpp>

namespace balsa::visualization::scene_graph {
template<unsigned int D, typename T>
struct EmbeddingTraits;

namespace concepts {
    // transformation type
    template<typename TT>
    concept TransformationTraits = requires(typename TT::projection_type p, typename TT::transform_type t, typename TT::point_type, typename TT::homogeneous_type h) {

        { TT::matrix_product(t, h) } -> std::is_same_as<typename TT::homogeneous_type>;
    //        TT::matrix_product(p, TT::matrix_product(t,)
    }
}// namespace concepts

template <typename ET>
struct MatrixTransformationTraits {
};

template<unsigned int D, typename T>
struct MatrixTransformationTraits<EmbeddingTraits<D,T>>{
    using point_type = glm::vec<D, T>;
    using homogeneous_point_type = glm::vec<D + 1, T>;
    using vector_type = glm::vec<D, T>;
    using projection_type = glm::mat<3, D + 1, T>;
    using transform_type = glm::mat<D + 1, D + 1, T>;


    using as_homogeneous_point = [](const point_type &p) { return homogeneous_point_type(p, T(1)); };
    using as_homogeneous_vector = [](const vector_type &p) { return homogeneous_vector_type(p, T(0)); }
    using matrix_product = [](const point_type& a, const point_type& b) {
        return a * b;
    };
};
}// namespace balsa::visualization::scene_graph

#endif
