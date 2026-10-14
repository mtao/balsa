#pragma once
#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "AbstractFeature.hpp"
#include "Signal.hpp"
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
// Features are the extensibility mechanism: Camera, MeshInstance,
// BasicShading, Drawable etc. are all features that can be attached to
// any Object via emplace_feature<F>().  Shared data such as a
// MeshGeometry is not a feature; features refer to it.
//
// Objects have identity: children, features and signal listeners refer
// to them, so they are neither copyable nor movable.  Children are owned
// via unique_ptr; the parent pointer is non-owning.
//
// Signals report structural changes; see the Signals section.

class Object {
  public:
    explicit Object(std::string name = "");
    virtual ~Object();

    Object(const Object &) = delete;
    auto operator=(const Object &) -> Object & = delete;
    Object(Object &&) = delete;
    auto operator=(Object &&) -> Object & = delete;

    // ── Transform (decomposed TRS) ─────────────────────────────────

    // Translation
    [[nodiscard]] auto translation() const -> const Vec3f & {
        return _translation;
    }
    auto set_translation(const Vec3f &t) -> void { _translation = t; }

    // Rotation (stored as unit quaternion)
    [[nodiscard]] auto rotation() const -> const Quaternionf & {
        return _rotation;
    }
    auto set_rotation(const Quaternionf &q) -> void { _rotation = q; }

    // Rotation as Euler angles in degrees (XYZ / pitch-yaw-roll).
    // Returns (pitch, yaw, roll) in degrees.
    [[nodiscard]] auto rotation_euler() const -> Vec3f;

    // Set rotation from Euler angles in degrees (pitch, yaw, roll).
    auto set_rotation_euler(const Vec3f &degrees) -> void;

    // Scale factors (per-axis)
    [[nodiscard]] auto scale_factors() const -> const Vec3f & {
        return _scale;
    }
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
    [[nodiscard]] auto local_transform() const -> AffineTransformf;

    // Decompose an affine transform into T/R/S and store them.
    // Assumes the transform contains no shear (only translation,
    // rotation, and axis-aligned scale).  If the linear block has
    // negative determinant, one scale factor will be negative.
    auto set_from_transform(const AffineTransformf &xf) -> void;

    // Accumulated world transform: product of all ancestors' local
    // transforms, from root down to (and including) this node.
    [[nodiscard]] auto world_transform() const -> AffineTransformf;

    // ── Hierarchy ───────────────────────────────────────────────────

    [[nodiscard]] auto parent() const -> Object * { return _parent; }
    [[nodiscard]] auto children() const
        -> const std::vector<std::unique_ptr<Object>> & {
        return _children;
    }
    [[nodiscard]] auto children_count() const -> std::size_t {
        return _children.size();
    }

    // Create a new child Object with the given name.
    auto add_child(std::string name = "") -> Object &;

    // Take ownership of an existing Object and add it as a child.
    // Reparents: removes the object from its previous parent (if any).
    auto add_child(std::unique_ptr<Object> child) -> Object &;

    // Detach this Object from its parent, returning ownership.
    // Returns nullptr if this Object has no parent or is permanent.
    auto detach() -> std::unique_ptr<Object>;

    // ── Traversal ───────────────────────────────────────────────────
    //
    // fn / pred take the visited Object (const for const traversal).
    // A visitor returning bool controls the walk: for_each_descendant
    // skips a node's subtree when it returns false, for_each_ancestor
    // stops when it returns false.  A void visitor visits everything.

    // Every descendant (not this Object), depth-first, pre-order.
    template <typename Fn>
    auto for_each_descendant(Fn &&fn) -> void;
    template <typename Fn>
    auto for_each_descendant(Fn &&fn) const -> void;

    // The parent, grandparent, ... up to the root.
    template <typename Fn>
    auto for_each_ancestor(Fn &&fn) -> void;
    template <typename Fn>
    auto for_each_ancestor(Fn &&fn) const -> void;

    // The first descendant (pre-order) satisfying pred, or nullptr.
    template <typename Pred>
    [[nodiscard]] auto find_descendant(Pred &&pred) -> Object *;
    template <typename Pred>
    [[nodiscard]] auto find_descendant(Pred &&pred) const -> const Object *;

    // Whether this Object is a (strict) descendant of ancestor.
    [[nodiscard]] auto is_descendant_of(const Object &ancestor) const -> bool;

    // ── Features ────────────────────────────────────────────────────

    // Construct and attach a feature of type F.
    // F must derive from AbstractFeature.
    // Returns a reference to the newly created feature.
    template <typename F, typename... Args>
    auto emplace_feature(Args &&...args) -> F &;

    // Find the first attached feature of type F (exact or derived).
    // Returns nullptr if no such feature is found.
    template <typename F>
    [[nodiscard]] auto find_feature() -> F *;
    template <typename F>
    [[nodiscard]] auto find_feature() const -> const F *;

    // Access all features (for advanced iteration).
    [[nodiscard]] auto features() const
        -> const std::vector<std::unique_ptr<AbstractFeature>> & {
        return _features;
    }

    // Remove a feature by pointer.  Returns true if the feature was found
    // and removed.  The feature is destroyed.
    auto remove_feature(AbstractFeature *feature) -> bool;

    // ── Signals ─────────────────────────────────────────────────────
    //
    // Structural changes to this Object.  Anyone can connect; only the
    // Object emits.
    //
    // When an Object is destroyed, `destroying` is emitted first; its own
    // features and children are then destroyed with it without
    // feature_removing / child_removing (each child emits its own
    // `destroying`).

    // After a feature is constructed and attached.
    Signal<Object, Object &, AbstractFeature &> feature_added;
    // Before a feature is removed and destroyed.
    Signal<Object, Object &, AbstractFeature &> feature_removing;
    // After a child is attached to this Object: (parent, child).
    Signal<Object, Object &, Object &> child_added;
    // Before a child is detached from this Object (detach() or
    // re-parenting): (parent, child).
    Signal<Object, Object &, Object &> child_removing;
    // At the start of this Object's destruction.
    Signal<Object, Object &> destroying;

    // ── Identity ────────────────────────────────────────────────────

    std::string name;
    bool visible = true;
    bool selectable = true;

    // If true, this Object cannot be deleted or detached from its parent.
    // Used for scene fixtures such as the default camera.
    bool permanent = false;

  private:
    // Remove child from _children without destroying it.
    auto release_child(Object &child) -> std::unique_ptr<Object>;

    Vec3f _translation{ 0.0f, 0.0f, 0.0f };
    Quaternionf _rotation{ 1, 0, 0, 0 }; // identity quaternion (w=1)
    Vec3f _scale{ 1.0f, 1.0f, 1.0f };
    Object *_parent = nullptr;
    std::vector<std::unique_ptr<Object>> _children;
    std::vector<std::unique_ptr<AbstractFeature>> _features;
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
    feature_added.emit(*this, ref);
    return ref;
}

template <typename F>
auto Object::find_feature() -> F * {
    for (auto &f : _features) {
        if (auto *p = dynamic_cast<F *>(f.get())) return p;
    }
    return nullptr;
}

template <typename F>
auto Object::find_feature() const -> const F * {
    for (const auto &f : _features) {
        if (const auto *p = dynamic_cast<const F *>(f.get())) return p;
    }
    return nullptr;
}

namespace detail {
    // Calls fn(node); a bool result says whether to continue (descend /
    // keep walking), a void result always continues.
    template <typename Fn, typename Node>
    auto visit_continues(Fn &fn, Node &node) -> bool {
        if constexpr (std::is_same_v<std::invoke_result_t<Fn &, Node &>,
                                     bool>) {
            return fn(node);
        } else {
            fn(node);
            return true;
        }
    }
} // namespace detail

template <typename Fn>
auto Object::for_each_descendant(Fn &&fn) -> void {
    for (auto &child : _children) {
        if (detail::visit_continues(fn, *child)) {
            child->for_each_descendant(fn);
        }
    }
}

template <typename Fn>
auto Object::for_each_descendant(Fn &&fn) const -> void {
    for (const auto &child : _children) {
        const Object &c = *child;
        if (detail::visit_continues(fn, c)) c.for_each_descendant(fn);
    }
}

template <typename Fn>
auto Object::for_each_ancestor(Fn &&fn) -> void {
    for (Object *p = _parent; p != nullptr; p = p->_parent) {
        if (!detail::visit_continues(fn, *p)) return;
    }
}

template <typename Fn>
auto Object::for_each_ancestor(Fn &&fn) const -> void {
    for (const Object *p = _parent; p != nullptr; p = p->_parent) {
        if (!detail::visit_continues(fn, *p)) return;
    }
}

template <typename Pred>
auto Object::find_descendant(Pred &&pred) -> Object * {
    for (auto &child : _children) {
        if (pred(*child)) return child.get();
        if (auto *found = child->find_descendant(pred)) return found;
    }
    return nullptr;
}

template <typename Pred>
auto Object::find_descendant(Pred &&pred) const -> const Object * {
    for (const auto &child : _children) {
        const Object &c = *child;
        if (pred(c)) return &c;
        if (const auto *found = c.find_descendant(pred)) return found;
    }
    return nullptr;
}

} // namespace balsa::scene_graph
