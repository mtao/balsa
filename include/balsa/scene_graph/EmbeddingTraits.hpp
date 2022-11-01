#if !defined(BALSA_SCENE_GRAPH_EMBEDDING_TRAITS_HPP)
#define BALSA_SCENE_GRAPH_EMBEDDING_TRAITS_HPP

//#include "balsa/types/is_specialization_of.hpp"
#include <glm/matrix.hpp>

namespace balsa::scene_graph {


template<unsigned int D, typename T>
struct EmbeddingTraits {
    using scalar_type = T;
    constexpr static unsigned int embedding_dimension = D;

    using vector_type = glm::vec<D, T>;
    // good for matrix-vector multiplication
    using matrix_type = glm::mat<D, D, T>;
    // good for homogeneous coordinates
    using transformation_matrix_type = glm::mat<D + 1, D + 1, T>;
};

using EmbeddingTraits2F = EmbeddingTraits<2, float>;
using EmbeddingTraits2D = EmbeddingTraits<2, double>;
using EmbeddingTraits3F = EmbeddingTraits<3, float>;
using EmbeddingTraits3D = EmbeddingTraits<3, double>;

namespace concepts {
    template<typename T>
    concept EmbeddingTraits = std::is_scalar_v<typename T::scalar_type> && std::is_arithmetic_v<decltype(T::embedding_dimension)>;
    // Can't use specialization without P1985/P2098
    //= types::is_specialization_of_v<scene_graph::EmbeddingTraits, T>;
}// namespace concepts

}// namespace balsa::scene_graph
#endif
