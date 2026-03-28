#pragma once

#include <span>
#include <typeindex>
#include <unordered_map>
#include <vector>

#include <sigslot/signal.hpp>

#include "AbstractFeature.hpp"
#include "Object.hpp"

namespace balsa::scene_graph {

// ── FeatureIndex ────────────────────────────────────────────────────
//
// Maintains a scene-level index that maps feature types to the set of
// Objects that carry a feature of that type.  Subscribes to Object
// signals (on_feature_added, on_feature_removing, on_child_added,
// on_child_removing) so the index stays up to date automatically.
//
// Usage:
//   FeatureIndex index;
//   index.track(root);   // recursively subscribe to root + all descendants
//
//   auto meshes = index.objects_with<MeshData>();  // O(1) lookup
//   for (Object* obj : meshes) { ... }
//
// Inherits sigslot::observer_st so that all signal connections are
// automatically severed when the FeatureIndex is destroyed.

class FeatureIndex : public sigslot::observer_st {
  public:
    FeatureIndex() = default;
    ~FeatureIndex() override = default;

    // Non-copyable, non-movable (observer_st manages connections).
    FeatureIndex(const FeatureIndex &) = delete;
    FeatureIndex &operator=(const FeatureIndex &) = delete;
    FeatureIndex(FeatureIndex &&) = delete;
    FeatureIndex &operator=(FeatureIndex &&) = delete;

    // Recursively subscribe to an Object and all its current
    // descendants.  Indexes every feature already present.
    void track(Object &root);

    // Stop tracking an Object (and its descendants).
    void untrack(Object &root);

    // Return all Objects that currently carry a feature of type F.
    template <typename F>
    auto objects_with() const -> std::span<Object *const>;

    // Return the count of Objects carrying feature type F.
    template <typename F>
    auto count() const -> std::size_t;

  private:
    void track_single(Object &obj);
    void untrack_single(Object &obj);

    // Signal handlers.
    void on_feature_added(Object &obj, AbstractFeature &feature);
    void on_feature_removing(Object &obj, AbstractFeature &feature);
    void on_child_added(Object &parent, Object &child);
    void on_child_removing(Object &parent, Object &child);

    // type_index -> vector of Object pointers carrying that feature type.
    std::unordered_map<std::type_index, std::vector<Object *>> _index;
};

// ── Template implementations ────────────────────────────────────────

template <typename F>
auto FeatureIndex::objects_with() const -> std::span<Object *const> {
    auto it = _index.find(std::type_index(typeid(F)));
    if (it == _index.end()) return {};
    return it->second;
}

template <typename F>
auto FeatureIndex::count() const -> std::size_t {
    auto it = _index.find(std::type_index(typeid(F)));
    if (it == _index.end()) return 0;
    return it->second.size();
}

} // namespace balsa::scene_graph
