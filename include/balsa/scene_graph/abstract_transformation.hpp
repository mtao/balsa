
#if !defined(BALSA_VISUALIZATION_SCENE_GRAPH_ABSTRACT_TRANSFORMATION_HPP)
#define BALSA_VISUALIZATION_SCENE_GRAPH_ABSTRACT_TRANSFORMATION_HPP

#include <concepts>
#include "embedding_traits.hpp"


namespace balsa::scene_graph {

template<concepts::embedding_traits ET>
class AbstractTransformation {
  public:
    using embedding_traits = ET;
    AbstractTransformation &reset_transformation() {
        do_reset_transformation();
        return *this;
    }
    virtual ~AbstractTransformation() = default;

    virtual typename embedding_traits::transformation_matrix_type as_matrix() const = 0;

  private:
    virtual void do_reset_transformation() = 0;
};

namespace concepts {
    template<typename T>
    concept abstract_transformation = concepts::embedding_traits<typename T::embedding_traits> && std::is_base_of_v<scene_graph::AbstractTransformation<typename T::embedding_traits>, T>;

};
}// namespace balsa::scene_graph

#endif

