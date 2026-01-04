#if !defined(BALSA_SCENE_GRAPH_FEATURE_HPP)
#define BALSA_SCENE_GRAPH_FEATURE_HPP

#include "embedding_traits.hpp"

namespace balsa::scene_graph {
template<concepts::embedding_traits ETraits>
class AbstractObject;


template<concepts::embedding_traits ETraits>
class AbstractFeature {
  public:
    using embedding_traits = ETraits;
    using abstract_object_type = AbstractObject<ETraits>;
    AbstractFeature() = default;
    AbstractFeature(AbstractFeature &) = delete;
    AbstractFeature(AbstractFeature &&) = delete;

    AbstractFeature &operator=(AbstractFeature &) = delete;
    AbstractFeature &operator=(AbstractFeature &&) = delete;

  private:
    friend abstract_object_type;
    AbstractFeature(abstract_object_type *o) : _object(o) {}
    void set_object(abstract_object_type &object) {
        _object = &object;
    }
    abstract_object_type *_object = nullptr;
};
}// namespace balsa::scene_graph
#endif
