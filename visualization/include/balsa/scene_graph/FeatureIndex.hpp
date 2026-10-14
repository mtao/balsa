#pragma once
#include <cstddef>
#include <span>
#include <typeindex>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <sigslot/signal.hpp>

#include "AbstractFeature.hpp"
#include "Object.hpp"

namespace balsa::scene_graph {

// ── FeatureIndex ────────────────────────────────────────────────────
//
// Scene-level index from feature type to the Objects carrying a feature
// of exactly that type, kept up to date through Object signals:
//
//   FeatureIndex index;
//   index.track(root);  // root and all its descendants, present and future
//   for (Object *obj : index.objects_with<MeshInstance>()) { ... }
//
// Queries match the feature's exact (dynamic) type; a query for a base
// class such as VulkanDrawable finds nothing.  Each Object is listed once
// per type, however many features of that type it has, in the order it
// gained its first one.  Objects leave the index when they are untracked,
// detached from a tracked parent, lose their last feature of a type, or
// are destroyed.  Destroying the index disconnects it from every Object.

class FeatureIndex : public sigslot::observer_st {
  public:
    FeatureIndex() = default;
    ~FeatureIndex() override;

    // Connections refer to this index, so it can't be copied or moved.
    FeatureIndex(const FeatureIndex &) = delete;
    auto operator=(const FeatureIndex &) -> FeatureIndex & = delete;
    FeatureIndex(FeatureIndex &&) = delete;
    auto operator=(FeatureIndex &&) -> FeatureIndex & = delete;

    // Index root and its descendants, including ones added later.
    // Tracking an already tracked Object does nothing.
    auto track(Object &root) -> void;
    // Stop indexing root and its descendants.
    auto untrack(Object &root) -> void;
    [[nodiscard]] auto is_tracked(const Object &obj) const -> bool;

    // Objects carrying a feature of exactly type F.  Valid until the
    // index next changes.
    template <typename F>
    [[nodiscard]] auto objects_with() const -> std::span<Object *const>;

    // Number of Objects carrying a feature of exactly type F.
    template <typename F>
    [[nodiscard]] auto count() const -> std::size_t {
        return objects_with<F>().size();
    }

  private:
    [[nodiscard]] auto objects_with(std::type_index type) const
        -> std::span<Object *const>;

    auto track_single(Object &obj) -> void;
    auto untrack_single(Object &obj) -> void;
    auto add(Object &obj, std::type_index type) -> void;
    auto remove(Object &obj, std::type_index type) -> void;

    // Signal handlers.
    auto on_feature_added(Object &obj, AbstractFeature &feature) -> void;
    auto on_feature_removing(Object &obj, AbstractFeature &feature) -> void;
    auto on_child_added(Object &parent, Object &child) -> void;
    auto on_child_removing(Object &parent, Object &child) -> void;
    auto on_destroying(Object &obj) -> void;

    std::unordered_map<std::type_index, std::vector<Object *>> _index;
    std::unordered_set<const Object *> _tracked;
};

template <typename F>
auto FeatureIndex::objects_with() const -> std::span<Object *const> {
    return objects_with(std::type_index(typeid(F)));
}

} // namespace balsa::scene_graph
