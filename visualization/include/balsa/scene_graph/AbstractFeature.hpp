#if !defined(BALSA_SCENE_GRAPH_ABSTRACT_FEATURE_HPP)
#define BALSA_SCENE_GRAPH_ABSTRACT_FEATURE_HPP

namespace balsa::scene_graph {

class Object;

// ── AbstractFeature ─────────────────────────────────────────────────
//
// Base class for all features (components) that can be attached to an
// Object.  Each feature holds a back-reference to its owning Object,
// set automatically by Object::emplace_feature().
//
// Non-copyable, non-movable — features are owned (via unique_ptr) by
// the Object they are attached to.

class AbstractFeature {
  public:
    virtual ~AbstractFeature() = default;

    Object &object() { return *_object; }
    const Object &object() const { return *_object; }

  protected:
    AbstractFeature() = default;

    AbstractFeature(const AbstractFeature &) = delete;
    AbstractFeature &operator=(const AbstractFeature &) = delete;
    AbstractFeature(AbstractFeature &&) = delete;
    AbstractFeature &operator=(AbstractFeature &&) = delete;

  private:
    friend class Object;
    Object *_object = nullptr;
};

}// namespace balsa::scene_graph

#endif
