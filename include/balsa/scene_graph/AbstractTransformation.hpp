
#if !defined(BALSA_VISUALIZATION_SCENE_GRAPH_ABSTRACT_TRANSFORMATION_HPP)
#define BALSA_VISUALIZATION_SCENE_GRAPH_ABSTRACT_TRANSFORMATION_HPP

#include <concepts>
#include "EmbeddingTraits.hpp"


namespace balsa::scene_graph {

template<concepts::EmbeddingTraits ET>
class AbstractTransformation {
  public:
    using EmbeddingTraits = ET;
    AbstractTransformation &reset_transformation() {
        do_reset_transformation();
        return *this;
    }
    virtual ~AbstractTransformation() = default;

    virtual typename EmbeddingTraits::transformation_matrix_type as_matrix() const = 0;

  private:
    virtual void do_reset_transformation() = 0;
};

namespace concepts {
    template<typename T>
    concept AbstractTransformation = concepts::EmbeddingTraits<typename T::EmbeddingTraits> && std::is_base_of_v<scene_graph::AbstractTransformation<typename T::EmbeddingTraits>, T>;

};
}// namespace balsa::scene_graph

#endif

