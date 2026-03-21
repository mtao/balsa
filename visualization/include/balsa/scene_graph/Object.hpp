#if !defined(BALSA_SCENE_GRAPH_OBJECT_HPP_NEW)
#define BALSA_SCENE_GRAPH_OBJECT_HPP_NEW

#include <memory>
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
//   - A local affine transform (AffineTransformf, column-major 4x4)
//   - A parent pointer and an ordered list of children
//   - A collection of features (components) attached to it
//   - A name and a visibility flag
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

    // ── Transform ───────────────────────────────────────────────────

    const AffineTransformf &local_transform() const { return _local_transform; }
    void set_local_transform(const AffineTransformf &t) { _local_transform = t; }

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

    // ── Identity ────────────────────────────────────────────────────

    std::string name;
    bool visible = true;

  private:
    AffineTransformf _local_transform;// default-constructs to identity
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
