#if !defined(BALSA_SCENE_GRAPH_OBJECT_HPP_NEW)
#define BALSA_SCENE_GRAPH_OBJECT_HPP_NEW

#include <cmath>
#include <memory>
#include <numbers>
#include <string>
#include <type_traits>
#include <typeindex>
#include <unordered_map>
#include <vector>

#include "AbstractFeature.hpp"
#include "types.hpp"

namespace balsa::scene_graph {

// ── Object ──────────────────────────────────────────────────────────
//
// The universal scene graph node.  Every node in the scene graph is an
// Object with:
//   - Decomposed TRS local transform (Translation, Rotation as
//     quaternion, Scale) following the Blender convention
//   - A parent pointer and an ordered list of children
//   - A collection of features (components) attached to it
//   - A name, visibility flag, and selectability flag
//
// The local transform is composed as T * R * S.  local_transform()
// returns the composed AffineTransformf by value; there is no stored
// 4×4 matrix.
//
// Features are the extensibility mechanism: Camera, MeshData, Drawable
// etc. are all features that can be attached to any Object via
// emplace_feature<F>().
//
// Non-copyable.  Children are owned via unique_ptr; the parent pointer
// is non-owning.  Objects can be moved.

class Object {
  public:
    explicit Object(std::string name = "");
    virtual ~Object();

    // Non-copyable
    Object(const Object &) = delete;
    Object &operator=(const Object &) = delete;

    // Movable
    Object(Object &&) noexcept;
    Object &operator=(Object &&) noexcept;

    // ── Transform (decomposed TRS) ─────────────────────────────────

    // Translation
    auto translation() const -> const Vec3f & { return _translation; }
    auto set_translation(const Vec3f &t) -> void { _translation = t; }

    // Rotation (stored as unit quaternion)
    auto rotation() const -> const Quaternionf & { return _rotation; }
    auto set_rotation(const Quaternionf &q) -> void { _rotation = q; }

    // Rotation as Euler angles in degrees (XYZ / pitch-yaw-roll).
    // Returns (pitch, yaw, roll) in degrees.
    auto rotation_euler() const -> Vec3f;

    // Set rotation from Euler angles in degrees (pitch, yaw, roll).
    auto set_rotation_euler(const Vec3f &degrees) -> void;

    // Scale factors (per-axis)
    auto scale_factors() const -> const Vec3f & { return _scale; }
    auto set_scale_factors(const Vec3f &s) -> void { _scale = s; }

    // ── Convenience mutators ────────────────────────────────────────

    // Add to current translation.
    auto translate(const Vec3f &delta) -> void;

    // Post-multiply rotation by the given quaternion.
    auto rotate(const Quaternionf &q) -> void;

    // Set uniform scale on all axes.
    auto set_uniform_scale(float s) -> void;

    // Reset TRS to identity (translation=0, rotation=identity, scale=1).
    auto reset_transform() -> void;

    // ── Composed transforms ─────────────────────────────────────────

    // Compose T * R * S and return the resulting affine transform.
    // Computed on the fly — not cached.
    auto local_transform() const -> AffineTransformf;

    // Decompose an affine transform into T/R/S and store them.
    // Assumes the transform contains no shear (only translation,
    // rotation, and axis-aligned scale).  If the linear block has
    // negative determinant, one scale factor will be negative.
    auto set_from_transform(const AffineTransformf &xf) -> void;

    // Accumulated world transform: product of all ancestors' local
    // transforms, from root down to (and including) this node.
    auto world_transform() const -> AffineTransformf;

    // ── Hierarchy ───────────────────────────────────────────────────

    auto parent() const -> Object * { return _parent; }
    auto children() const -> const std::vector<std::unique_ptr<Object>> & {
        return _children;
    }
    auto children_count() const -> std::size_t { return _children.size(); }

    // Create a new child Object with the given name.
    auto add_child(std::string name = "") -> Object &;

    // Take ownership of an existing Object and add it as a child.
    // Reparents: removes the object from its previous parent (if any).
    auto add_child(std::unique_ptr<Object> child) -> Object &;

    // Detach this Object from its parent, returning ownership.
    // Returns nullptr if this Object has no parent.
    auto detach() -> std::unique_ptr<Object>;

    // ── Traversal ───────────────────────────────────────────────────

    // Visit every descendant (depth-first, pre-order).
    // If fn returns bool, returning false prunes the subtree.
    // If fn returns void, all descendants are visited.
    template <typename Fn>
    auto for_each_descendant(Fn &&fn) -> void;

    template <typename Fn>
    auto for_each_descendant(Fn &&fn) const -> void;

    // Walk the ancestor chain (parent, grandparent, ...).
    // If fn returns bool, returning false stops traversal.
    template <typename Fn>
    auto for_each_ancestor(Fn &&fn) const -> void;

    // Return the first descendant (DFS pre-order) satisfying pred,
    // or nullptr if none.
    template <typename Pred>
    auto find_descendant(Pred &&pred) -> Object *;

    template <typename Pred>
    auto find_descendant(Pred &&pred) const -> const Object *;

    // ── Features ────────────────────────────────────────────────────

    // Construct and attach a feature of type F.
    // F must derive from AbstractFeature.
    // Returns a reference to the newly created feature.
    template <typename F, typename... Args>
    auto emplace_feature(Args &&...args) -> F &;

    // Find the first attached feature of type F (exact or derived).
    // Returns nullptr if no such feature is found.
    template <typename F>
    auto find_feature() -> F *;

    template <typename F>
    auto find_feature() const -> const F *;

    // Access all features (for advanced iteration).
    auto features() const -> const std::vector<std::unique_ptr<AbstractFeature>> & {
        return _features;
    }

    // Remove a feature by pointer.  Returns true if the feature was found
    // and removed.  The feature is destroyed.
    auto remove_feature(AbstractFeature *feature) -> bool;

    // ── Identity ────────────────────────────────────────────────────

    std::string name;
    bool visible = true;
    bool selectable = true;

    // If true, this Object cannot be deleted or detached from its parent.
    // Used for scene fixtures such as the default camera.
    bool permanent = false;

    // ── Signals ─────────────────────────────────────────────────────

    // Emitted after a feature is constructed and attached.
    sigslot::signal_st<Object &, AbstractFeature &> on_feature_added;

    // Emitted just before a feature is destroyed and removed.
    sigslot::signal_st<Object &, AbstractFeature &> on_feature_removing;

    // Emitted after a child Object is added to this node.
    sigslot::signal_st<Object &, Object &> on_child_added;

    // Emitted just before a child Object is removed from this node.
    sigslot::signal_st<Object &, Object &> on_child_removing;

  private:
    Vec3f _translation; // default: {0, 0, 0}
    Quaternionf _rotation{1, 0, 0, 0}; // identity quaternion (w=1)
    Vec3f _scale; // default: {1, 1, 1} — set in ctor
    Object *_parent = nullptr;
    std::vector<std::unique_ptr<Object>> _children;
    std::vector<std::unique_ptr<AbstractFeature>> _features;

    // O(1) lookup by exact type.  Keyed by typeid(F) used at
    // emplace_feature<F>() time.  Points into _features' unique_ptrs.
    std::unordered_map<std::type_index, AbstractFeature *> _feature_map;
};

// ── Template implementations ────────────────────────────────────────

template <typename F, typename... Args>
auto Object::emplace_feature(Args &&...args) -> F & {
    static_assert(std::is_base_of_v<AbstractFeature, F>,
                  "F must derive from AbstractFeature");
    auto ptr = std::make_unique<F>(std::forward<Args>(args)...);
    ptr->_object = this;
    F &ref = *ptr;
    _features.push_back(std::move(ptr));
    _feature_map[std::type_index(typeid(F))] = &ref;
    on_feature_added(*this, ref);
    return ref;
}

template <typename F>
auto Object::find_feature() -> F * {
    // Fast path: exact type match via map.
    if (auto it = _feature_map.find(std::type_index(typeid(F)));
        it != _feature_map.end()) {
        return static_cast<F *>(it->second);
    }
    // Slow path: base-class query via dynamic_cast scan.
    for (auto &f : _features) {
        if (auto *p = dynamic_cast<F *>(f.get())) return p;
    }
    return nullptr;
}

template <typename F>
auto Object::find_feature() const -> const F * {
    // Fast path: exact type match via map.
    if (auto it = _feature_map.find(std::type_index(typeid(F)));
        it != _feature_map.end()) {
        return static_cast<const F *>(it->second);
    }
    // Slow path: base-class query via dynamic_cast scan.
    for (auto &f : _features) {
        if (auto *p = dynamic_cast<const F *>(f.get())) return p;
    }
    return nullptr;
}

// ── Traversal implementations ───────────────────────────────────────

namespace detail {
    // Detect if Fn(Object&) returns bool.
    template <typename Fn, typename ObjRef>
    constexpr bool returns_bool_v =
        std::is_same_v<std::invoke_result_t<Fn, ObjRef>, bool>;
} // namespace detail

template <typename Fn>
auto Object::for_each_descendant(Fn &&fn) -> void {
    for (auto &child : _children) {
        if (!child) continue;
        if constexpr (detail::returns_bool_v<Fn &, Object &>) {
            if (!fn(*child)) continue; // false -> prune subtree
        } else {
            fn(*child);
        }
        child->for_each_descendant(fn);
    }
}

template <typename Fn>
auto Object::for_each_descendant(Fn &&fn) const -> void {
    for (auto &child : _children) {
        if (!child) continue;
        if constexpr (detail::returns_bool_v<Fn &, const Object &>) {
            if (!fn(*child)) continue;
        } else {
            fn(*child);
        }
        child->for_each_descendant(fn);
    }
}

template <typename Fn>
auto Object::for_each_ancestor(Fn &&fn) const -> void {
    for (Object *p = _parent; p != nullptr; p = p->_parent) {
        if constexpr (detail::returns_bool_v<Fn &, const Object &>) {
            if (!fn(*p)) return;
        } else {
            fn(*p);
        }
    }
}

template <typename Pred>
auto Object::find_descendant(Pred &&pred) -> Object * {
    for (auto &child : _children) {
        if (!child) continue;
        if (pred(*child)) return child.get();
        if (auto *found = child->find_descendant(pred)) return found;
    }
    return nullptr;
}

template <typename Pred>
auto Object::find_descendant(Pred &&pred) const -> const Object * {
    for (auto &child : _children) {
        if (!child) continue;
        if (pred(std::as_const(*child))) return child.get();
        if (auto *found = std::as_const(*child).find_descendant(pred))
            return found;
    }
    return nullptr;
}

} // namespace balsa::scene_graph

#endif
