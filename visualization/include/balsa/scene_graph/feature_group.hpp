#if !defined(BALSA_VISUALIZATION_SCENE_GRAPH_FEATURE_GROUP_HPP)
#define BALSA_VISUALIZATION_SCENE_GRAPH_FEATURE_GROUP_HPP

#include <vector>
#include "abstract_feature.hpp"
#include "embedding_traits.hpp"

namespace balsa::scene_graph {


template<concepts::embedding_traits ETraits>
class AbstractFeatureGroup {
  public:
    using scalar_type = typename ETraits::scalar_type;
    constexpr static unsigned int dimension = ETraits::embedding_dimension;
    using abstract_feature_type = AbstractFeature<ETraits>;

  private:
    template<concepts::embedding_traits, typename>
    friend class FeatureGroup;
    void add(abstract_feature_type &feature);
    void remove(abstract_feature_type &feature);


    std::vector<std::reference_wrapper<abstract_feature_type>> _features;
};

template<concepts::embedding_traits ETraits, typename FeatureType>
class FeatureGroup {
};
}// namespace balsa::scene_graph

#endif
