#if !defined(BALSA_SCENE_GRAPH_GROUPED_FEATURE_HPP)
#define BALSA_SCENE_GRAPH_GROUPED_FEATURE_HPP

#include "AbstractFeature.hpp"

namespace balsa::scene_graph {


template<concepts::EmbeddingTraits ETraits>
class AbstractGroupedFeature : public AbstractFeature<ETraits> {
    using EmbeddingTraits = ETraits;
};
}// namespace balsa::scene_graph
#endif
