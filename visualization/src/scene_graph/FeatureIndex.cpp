#include "balsa/scene_graph/FeatureIndex.hpp"

#include <algorithm>

namespace balsa::scene_graph {

namespace {
    auto type_of(const AbstractFeature &feature) -> std::type_index {
        return std::type_index(typeid(feature));
    }
} // namespace

FeatureIndex::~FeatureIndex() { disconnect_all(); }

// ── Tracking ────────────────────────────────────────────────────────

auto FeatureIndex::track(Object &root) -> void {
    track_single(root);
    root.for_each_descendant([this](Object &obj) { track_single(obj); });
}

auto FeatureIndex::untrack(Object &root) -> void {
    root.for_each_descendant([this](Object &obj) { untrack_single(obj); });
    untrack_single(root);
}

auto FeatureIndex::is_tracked(const Object &obj) const -> bool {
    return _tracked.contains(&obj);
}

auto FeatureIndex::track_single(Object &obj) -> void {
    if (!_tracked.insert(&obj).second) return;
    for (const auto &feature : obj.features()) add(obj, type_of(*feature));

    obj.feature_added.connect(&FeatureIndex::on_feature_added, this);
    obj.feature_removing.connect(&FeatureIndex::on_feature_removing, this);
    obj.child_added.connect(&FeatureIndex::on_child_added, this);
    obj.child_removing.connect(&FeatureIndex::on_child_removing, this);
    obj.destroying.connect(&FeatureIndex::on_destroying, this);
}

auto FeatureIndex::untrack_single(Object &obj) -> void {
    if (_tracked.erase(&obj) == 0) return;
    obj.feature_added.disconnect(this);
    obj.feature_removing.disconnect(this);
    obj.child_added.disconnect(this);
    obj.child_removing.disconnect(this);
    obj.destroying.disconnect(this);
    for (const auto &feature : obj.features()) remove(obj, type_of(*feature));
}

// ── Index ───────────────────────────────────────────────────────────

auto FeatureIndex::objects_with(std::type_index type) const
    -> std::span<Object *const> {
    const auto it = _index.find(type);
    if (it == _index.end()) return {};
    return it->second;
}

auto FeatureIndex::add(Object &obj, std::type_index type) -> void {
    auto &objects = _index[type];
    if (std::ranges::find(objects, &obj) == objects.end()) {
        objects.push_back(&obj);
    }
}

auto FeatureIndex::remove(Object &obj, std::type_index type) -> void {
    const auto it = _index.find(type);
    if (it == _index.end()) return;
    std::erase(it->second, &obj);
    if (it->second.empty()) _index.erase(it);
}

// ── Signal handlers ─────────────────────────────────────────────────

auto FeatureIndex::on_feature_added(Object &obj, AbstractFeature &feature)
    -> void {
    add(obj, type_of(feature));
}

auto FeatureIndex::on_feature_removing(Object &obj, AbstractFeature &feature)
    -> void {
    // Keep obj listed while it has another feature of the same type.
    const auto type = type_of(feature);
    const auto same_type = std::ranges::count_if(
        obj.features(), [&](const auto &f) { return type_of(*f) == type; });
    if (same_type <= 1) remove(obj, type);
}

auto FeatureIndex::on_child_added(Object & /*parent*/, Object &child) -> void {
    track(child);
}

auto FeatureIndex::on_child_removing(Object & /*parent*/, Object &child)
    -> void {
    untrack(child);
}

auto FeatureIndex::on_destroying(Object &obj) -> void {
    // Descendants emit their own `destroying` as they are destroyed.
    untrack_single(obj);
}

} // namespace balsa::scene_graph
