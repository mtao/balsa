#if !defined(BALSA_VISUALIZATION_SCENE_GRAPH_TRANSFORMATIONS_MATRIX_TRANSFORMATION_HPP)
#define BALSA_VISUALIZATION_SCENE_GRAPH_TRANSFORMATIONS_MATRIX_TRANSFORMATION_HPP

#include <concepts>
#include "../AbstractTransformation.hpp"
#include <glm/ext/matrix_transform.hpp>


namespace balsa::scene_graph::transformations {

template<concepts::EmbeddingTraits ET>
class MatrixTransformation : public AbstractTransformation<ET> {
  public:
    using EmbeddingTraits = ET;
    // glm::mat<D+1,D+1,T>;
    using matrix_type = typename EmbeddingTraits::transformation_matrix_type;


    matrix_type as_matrix() const override final { return _matrix; }

  private:
    matrix_type _matrix;
    void do_reset_transformation() override final {
        _matrix = glm::identity<matrix_type>();
    }
};

}// namespace balsa::scene_graph::transformations

#endif

