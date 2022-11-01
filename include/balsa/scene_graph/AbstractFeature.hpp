#if !defined(BALSA_SCENE_GRAPH_FEATURE_HPP)
#define BALSA_SCENE_GRAPH_FEATURE_HPP

#include "EmbeddingTraits.hpp"

namespace balsa::scene_graph {
template<concepts::EmbeddingTraits ETraits>
class AbstractObject;


template<concepts::EmbeddingTraits ETraits>
class AbstractFeature {
  public:
    using EmbeddingTraits = ETraits;
    using AbstractObject_type = AbstractObject<ETraits>;
    AbstractFeature() = default;
    AbstractFeature(AbstractFeature &) = delete;
    AbstractFeature(AbstractFeature &&) = delete;

    AbstractFeature &operator=(AbstractFeature &) = delete;
    AbstractFeature &operator=(AbstractFeature &&) = delete;

  private:
    friend AbstractObject_type;
    AbstractFeature(AbstractObject_type *o) : _object(o) {}
    void set_object(AbstractObject_type &object) {
        _object = &object;
    }
    AbstractObject_type *_object = nullptr;
};
}// namespace balsa::scene_graph
#endif
