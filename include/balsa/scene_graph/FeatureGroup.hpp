#if !defined(BALSA_VISUALIZATION_SCENE_GRAPH_FEATURE_GROUP_HPP)
#define BALSA_VISUALIZATION_SCENE_GRAPH_FEATURE_GROUP_HPP

#include <vector>
#include "AbstractFeature.hpp"
#include "EmbeddingTraits.hpp"

namespace balsa::scene_graph {


template<concepts::EmbeddingTraits EmbeddingTraits>
class AbstractFeatureGroup {
  public:
    using scalar_type = typename EmbeddingTraits::Scalar;
    constexpr static int dimension = EmbeddingTraits::Dim;
    using abstract_feature_type = AbstractFeature<EmbeddingTraits>;

  private:
    template<int, typename, typename>
    friend class FeatureGroup;
    void add(abstract_feature_type &feature);
    void remove(abstract_feature_type &feature);


    std::vector<std::reference_wrapper<abstract_feature_type>> _features;
};

template<unsigned int D, typename FeatureType, typename Scalar>
class FeatureGroup {
};
}// namespace balsa::scene_graph
#endif
