#if !defined(BALSA_SCENE_GRAPH_ABSTRACT_FEATURE_HPP)
#define BALSA_SCENE_GRAPH_ABSTRACT_FEATURE_HPP

#include <sigslot/signal.hpp>

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

    auto object() -> Object & { return *_object; }
    auto object() const -> const Object & { return *_object; }

    // Signal emitted when this feature's data has changed.
    // Subscribers receive a reference to the modified feature.
    sigslot::signal_st<AbstractFeature &> on_modified;

    // Call this from derived classes after mutating internal state.
    auto mark_modified() -> void { on_modified(*this); }

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

} // namespace balsa::scene_graph

#endif
