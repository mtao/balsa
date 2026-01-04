#if !defined(BALSA_SCENE_GRAPH_EMBEDDING_TRAITS_HPP)
#define BALSA_SCENE_GRAPH_EMBEDDING_TRAITS_HPP

//#include "balsa/types/is_specialization_of.hpp"
#include <glm/matrix.hpp>

namespace balsa::scene_graph {


template<unsigned int D, typename T>
struct embedding_traits {
    using scalar_type = T;
    constexpr static unsigned int embedding_dimension = D;

    using vector_type = glm::vec<D, T>;
    // good for matrix-vector multiplication
    using matrix_type = glm::mat<D, D, T>;
    // good for homogeneous coordinates
    using transformation_matrix_type = glm::mat<D + 1, D + 1, T>;
};

using embedding_traits2F = embedding_traits<2, float>;
using embedding_traits2D = embedding_traits<2, double>;
using embedding_traits3F = embedding_traits<3, float>;
using embedding_traits3D = embedding_traits<3, double>;

namespace concepts {
    template<typename T>
    concept embedding_traits = std::is_scalar_v<typename T::scalar_type> && std::is_arithmetic_v<decltype(T::embedding_dimension)>;
    // Can't use specialization without P1985/P2098
    //= types::is_specialization_of_v<scene_graph::embedding_traits, T>;
}// namespace concepts

}// namespace balsa::scene_graph
#endif
