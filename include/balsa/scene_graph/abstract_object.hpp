#if !defined(BALSA_SCENE_GRAPH_ABSTRACT_OBJECT_HPP)
#define BALSA_SCENE_GRAPH_ABSTRACT_OBJECT_HPP

#include "abstract_feature.hpp"

#include <vector>
#include <memory>
#include <list>

namespace balsa::scene_graph {
template<concepts::embedding_traits ETraits>
class AbstractFeature;


template<concepts::embedding_traits ETraits>
class AbstractObject {
  public:
    using embedding_traits = ETraits;
    using abstract_feature_type = AbstractFeature<ETraits>;

    template<typename FeatureType, typename... Args>
    FeatureType &add_feature(Args &&...args);

  private:
    std::vector<std::unique_ptr<abstract_feature_type>> _features;
};

template<concepts::embedding_traits ETraits>
template<typename FeatureType, typename... Args>
auto AbstractObject<ETraits>::add_feature(Args &&...args) -> FeatureType & {
    auto new_ptr = new FeatureType(std::forward<Args>(args)...);
    new_ptr->set_object(*this);
    _features.emplace_back(new_ptr);
    return *new_ptr;
}
}// namespace balsa::scene_graph
#endif
