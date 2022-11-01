#if !defined(BALSA_SCENE_GRAPH_OBJECT_HPP)
#define BALSA_SCENE_GRAPH_OBJECT_HPP

#include <memory>
#include <vector>
#include "EmbeddingTraits.hpp"
#include "AbstractObject.hpp"
#include "AbstractTransformation.hpp"
#include <glm/mat4x4.hpp>

namespace balsa::scene_graph {

template<concepts::AbstractTransformation TransformationType>
class Object : public std::enable_shared_from_this<Object<TransformationType>>
  , TransformationType
  , AbstractObject<typename TransformationType::EmbeddingTraits> {
  public:
    using EmbeddingTraits = typename TransformationType::EmbeddingTraits;
    using Ptr = std::shared_ptr<Object>;
    using ConstPtr = std::shared_ptr<const Object>;
    using AbstractObject<EmbeddingTraits>::add_feature;
    using transformation_type = TransformationType;
    Ptr get_ptr() {
        return std::enable_shared_from_this<Object>::shared_from_this();
    }
    ConstPtr get_ptr() const {
        return std::enable_shared_from_this<Object>::shared_from_this();
    }

    Object(const Ptr &parent = nullptr) : _parent(parent) {}

    const Ptr parent() const { return _parent; }
    const std::vector<Ptr> &children() const { return _children; }
    size_t children_size() const { return _children.size(); }


    void set_parent(Ptr parent);
    void add_child(Ptr);

    template<typename T, typename... Args>
    std::shared_ptr<T> emplace_child(Args &&...args);

  private:
    Ptr _parent;
    std::vector<Ptr> _children;
};


template<concepts::AbstractTransformation TransformationType>
void Object<TransformationType>::set_parent(Ptr parent) {
    _parent = parent;
}
template<concepts::AbstractTransformation TransformationType>
void Object<TransformationType>::add_child(Ptr ptr) {
    _children.emplace_back(std::move(ptr))->set_parent(get_ptr());
}
template<concepts::AbstractTransformation TransformationType>
template<typename T, typename... Args>
std::shared_ptr<T> Object<TransformationType>::emplace_child(Args &&...args) {
    auto ptr = std::shared_ptr<T>(new T(std::forward<Args>(args)...));
    add_child(ptr);
    return ptr;
}
}// namespace balsa::scene_graph
#endif
