#if !defined(BALSA_SCENE_GRAPH_DRAWABLE_HPP)
#define BALSA_SCENE_GRAPH_DRAWABLE_HPP

#include "AbstractFeature.hpp"
#include "DrawableGroup.hpp"

namespace balsa::scene_graph {

// ── Drawable ────────────────────────────────────────────────────────
//
// A feature that can be drawn.  Upon construction it registers itself
// with a DrawableGroup; upon destruction it unregisters.
//
// This is the base class for renderer-specific drawables (e.g.
// VulkanMeshDrawable).  Subclasses implement the actual drawing logic.

class Drawable : public AbstractFeature {
  public:
    explicit Drawable(DrawableGroup &group)
      : _group(&group) {
        _group->add(*this);
    }

    ~Drawable() override {
        if (_group) {
            _group->remove(*this);
        }
    }

    // Non-copyable (inherited), non-movable.
    Drawable(Drawable &&) = delete;
    Drawable &operator=(Drawable &&) = delete;

    DrawableGroup &group() { return *_group; }
    const DrawableGroup &group() const { return *_group; }

  private:
    DrawableGroup *_group;
};

}// namespace balsa::scene_graph

#endif
