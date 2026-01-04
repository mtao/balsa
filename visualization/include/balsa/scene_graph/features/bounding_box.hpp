#if !defined(BALSA_SCENE_GRAPH_FEATURES_BOUNDING_BOX_HPP)
#define BALSA_SCENE_GRAPH_FEATURES_BOUNDING_BOX_HPP

#include <Eigen/Geometry>
#include "../abstract_feature.hpp"
#include <iostream>
#include "../embedding_traits.hpp"

namespace balsa::scene_graph::features {


template<concepts::embedding_traits ETraits>
class BoundingBoxNode : public AbstractFeature<ETraits> {
  public:
    using embedding_traits = ETraits;
    using bounding_box_type = Eigen::AlignedBox<typename ETraits::scalar_type, ETraits::embedding_dimension>;

    virtual bounding_box_type bounding_box() const;
    virtual ~BoundingBoxNode() = default;
    void add_child(const BoundingBoxNode *n);
    const std::vector<const BoundingBoxNode *> &children() const { return _children; }


  private:
    std::vector<const BoundingBoxNode *> _children;
};
template<concepts::embedding_traits ETraits>
auto BoundingBoxNode<ETraits>::bounding_box() const -> bounding_box_type {
    bounding_box_type bbox;
    for (auto &&child : _children) {
        auto bb = child->bounding_box();
        std::cout << bb.min().transpose() << " => " << bb.max().transpose() << std::endl;
        bbox.extend(bb);
    }
    return bbox;
}

template<concepts::embedding_traits ETraits>
void BoundingBoxNode<ETraits>::add_child(const BoundingBoxNode *n) {
    std::cout << "Adding bounding box child!" << std::endl;
    _children.emplace_back(n);
}

// caches the local bounding box value
template<concepts::embedding_traits ETraits>
class CachedBoundingBoxNode : public BoundingBoxNode<ETraits> {
  public:
    using bounding_box_type = typename BoundingBoxNode<ETraits>::bounding_box_type;

    bounding_box_type bounding_box() const override final;

    void set_bounding_box(const bounding_box_type &bb) { _bbox = bb; }
    void update_from_children();

  private:
    bounding_box_type _bbox;
};
template<concepts::embedding_traits ETraits>
auto CachedBoundingBoxNode<ETraits>::bounding_box() const -> bounding_box_type {
    return _bbox;
}

template<concepts::embedding_traits ETraits>
void CachedBoundingBoxNode<ETraits>::update_from_children() {
    _bbox = BoundingBoxNode<ETraits>::bounding_box();
}
}// namespace balsa::scene_graph::features
#endif
