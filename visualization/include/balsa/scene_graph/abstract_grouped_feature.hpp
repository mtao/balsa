#if !defined(BALSA_SCENE_GRAPH_GROUPED_FEATURE_HPP)
#define BALSA_SCENE_GRAPH_GROUPED_FEATURE_HPP

#include "abstract_feature.hpp"

namespace balsa::scene_graph {


template<concepts::embedding_traits ETraits>
class AbstractGroupedFeature : public AbstractFeature<ETraits> {
    using embedding_traits = ETraits;
};
}// namespace balsa::scene_graph
#endif
