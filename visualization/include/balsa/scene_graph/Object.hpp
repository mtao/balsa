#if !defined(BALSA_SCENE_GRAPH_OBJECT_HPP_NEW)
#define BALSA_SCENE_GRAPH_OBJECT_HPP_NEW

#include <cmath>
#include <memory>
#include <numbers>
#include <string>
#include <type_traits>
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
    const Vec3f &translation() const { return _translation; }
    void set_translation(const Vec3f &t) { _translation = t; }

    // Rotation (stored as unit quaternion)
    const Quaternionf &rotation() const { return _rotation; }
    void set_rotation(const Quaternionf &q) { _rotation = q; }

    // Rotation as Euler angles in degrees (XYZ / pitch-yaw-roll).
    // Returns (pitch, yaw, roll) in degrees.
    Vec3f rotation_euler() const;

    // Set rotation from Euler angles in degrees (pitch, yaw, roll).
    void set_rotation_euler(const Vec3f &degrees);

    // Scale factors (per-axis)
    const Vec3f &scale_factors() const { return _scale; }
    void set_scale_factors(const Vec3f &s) { _scale = s; }

    // ── Convenience mutators ────────────────────────────────────────

    // Add to current translation.
    void translate(const Vec3f &delta);

    // Post-multiply rotation by the given quaternion.
    void rotate(const Quaternionf &q);

    // Set uniform scale on all axes.
    void set_uniform_scale(float s);

    // Reset TRS to identity (translation=0, rotation=identity, scale=1).
    void reset_transform();

    // ── Composed transforms ─────────────────────────────────────────

    // Compose T * R * S and return the resulting affine transform.
    // Computed on the fly — not cached.
    AffineTransformf local_transform() const;

    // Decompose an affine transform into T/R/S and store them.
    // Assumes the transform contains no shear (only translation,
    // rotation, and axis-aligned scale).  If the linear block has
    // negative determinant, one scale factor will be negative.
    void set_from_transform(const AffineTransformf &xf);

    // Accumulated world transform: product of all ancestors' local
    // transforms, from root down to (and including) this node.
    AffineTransformf world_transform() const;

    // ── Hierarchy ───────────────────────────────────────────────────

    Object *parent() const { return _parent; }
    const std::vector<std::unique_ptr<Object>> &children() const { return _children; }
    std::size_t children_count() const { return _children.size(); }

    // Create a new child Object with the given name.
    Object &add_child(std::string name = "");

    // Take ownership of an existing Object and add it as a child.
    // Reparents: removes the object from its previous parent (if any).
    Object &add_child(std::unique_ptr<Object> child);

    // Detach this Object from its parent, returning ownership.
    // Returns nullptr if this Object has no parent.
    std::unique_ptr<Object> detach();

    // ── Features ────────────────────────────────────────────────────

    // Construct and attach a feature of type F.
    // F must derive from AbstractFeature.
    // Returns a reference to the newly created feature.
    template<typename F, typename... Args>
    F &emplace_feature(Args &&...args);

    // Find the first attached feature of type F (exact or derived).
    // Returns nullptr if no such feature is found.
    template<typename F>
    F *find_feature();

    template<typename F>
    const F *find_feature() const;

    // Access all features (for advanced iteration).
    const std::vector<std::unique_ptr<AbstractFeature>> &features() const { return _features; }

    // Remove a feature by pointer.  Returns true if the feature was found
    // and removed.  The feature is destroyed.
    bool remove_feature(AbstractFeature *feature);

    // ── Identity ────────────────────────────────────────────────────

    std::string name;
    bool visible = true;
    bool selectable = true;

  private:
    Vec3f _translation;// default: {0, 0, 0}
    Quaternionf _rotation{ 1, 0, 0, 0 };// identity quaternion (w=1)
    Vec3f _scale;// default: {1, 1, 1} — set in ctor
    Object *_parent = nullptr;
    std::vector<std::unique_ptr<Object>> _children;
    std::vector<std::unique_ptr<AbstractFeature>> _features;
};


// ── Template implementations ────────────────────────────────────────

template<typename F, typename... Args>
F &Object::emplace_feature(Args &&...args) {
    static_assert(std::is_base_of_v<AbstractFeature, F>,
                  "F must derive from AbstractFeature");
    auto ptr = std::make_unique<F>(std::forward<Args>(args)...);
    ptr->_object = this;
    F &ref = *ptr;
    _features.push_back(std::move(ptr));
    return ref;
}

template<typename F>
F *Object::find_feature() {
    for (auto &f : _features) {
        if (auto *p = dynamic_cast<F *>(f.get())) return p;
    }
    return nullptr;
}

template<typename F>
const F *Object::find_feature() const {
    for (auto &f : _features) {
        if (auto *p = dynamic_cast<const F *>(f.get())) return p;
    }
    return nullptr;
}

}// namespace balsa::scene_graph

#endif
