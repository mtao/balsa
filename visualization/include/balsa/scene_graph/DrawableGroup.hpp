#if !defined(BALSA_SCENE_GRAPH_DRAWABLE_GROUP_HPP)
#define BALSA_SCENE_GRAPH_DRAWABLE_GROUP_HPP

#include <cstddef>
#include <vector>

namespace balsa::scene_graph {

class Drawable;

// ── DrawableGroup ───────────────────────────────────────────────────
//
// A flat collection of Drawable pointers.  Drawables auto-register
// with a group at construction and auto-unregister at destruction.
//
// The renderer iterates a DrawableGroup rather than walking the scene
// tree, making draw dispatch O(drawable_count) with no traversal.

class DrawableGroup {
  public:
    DrawableGroup() = default;
    ~DrawableGroup() = default;

    // Non-copyable, movable.
    DrawableGroup(const DrawableGroup &) = delete;
    DrawableGroup &operator=(const DrawableGroup &) = delete;
    DrawableGroup(DrawableGroup &&) noexcept = default;
    DrawableGroup &operator=(DrawableGroup &&) noexcept = default;

    // ── Iteration ───────────────────────────────────────────────────

    auto begin() { return _drawables.begin(); }
    auto end() { return _drawables.end(); }
    auto begin() const { return _drawables.begin(); }
    auto end() const { return _drawables.end(); }

    std::size_t size() const { return _drawables.size(); }
    bool empty() const { return _drawables.empty(); }

  private:
    friend class Drawable;
    void add(Drawable &d);
    void remove(Drawable &d);

    std::vector<Drawable *> _drawables;
};

}// namespace balsa::scene_graph

#endif
