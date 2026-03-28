#include "balsa/scene_graph/FeatureIndex.hpp"

#include <algorithm>

namespace balsa::scene_graph {

// ── Public API ──────────────────────────────────────────────────────

void FeatureIndex::track(Object &root) {
    track_single(root);
    for (auto &child_ptr : root.children()) {
        if (child_ptr) track(*child_ptr);
    }
}

void FeatureIndex::untrack(Object &root) {
    for (auto &child_ptr : root.children()) {
        if (child_ptr) untrack(*child_ptr);
    }
    untrack_single(root);
}

// ── Internals ───────────────────────────────────────────────────────

void FeatureIndex::track_single(Object &obj) {
    // Index all features already present on this object.
    for (auto &f : obj.features()) {
        if (f) { _index[std::type_index(typeid(*f))].push_back(&obj); }
    }

    // Subscribe to future feature add/remove.
    obj.on_feature_added.connect(&FeatureIndex::on_feature_added, this);
    obj.on_feature_removing.connect(&FeatureIndex::on_feature_removing, this);

    // Subscribe to child add/remove so we auto-track new children.
    obj.on_child_added.connect(&FeatureIndex::on_child_added, this);
    obj.on_child_removing.connect(&FeatureIndex::on_child_removing, this);
}

void FeatureIndex::untrack_single(Object &obj) {
    // Disconnect all signals from this object to us.
    obj.on_feature_added.disconnect(this);
    obj.on_feature_removing.disconnect(this);
    obj.on_child_added.disconnect(this);
    obj.on_child_removing.disconnect(this);

    // Remove this object from the index.
    for (auto &f : obj.features()) {
        if (!f) continue;
        auto ti = std::type_index(typeid(*f));
        auto map_it = _index.find(ti);
        if (map_it == _index.end()) continue;
        auto &vec = map_it->second;
        vec.erase(std::remove(vec.begin(), vec.end(), &obj), vec.end());
        if (vec.empty()) _index.erase(map_it);
    }
}

// ── Signal handlers ─────────────────────────────────────────────────

void FeatureIndex::on_feature_added(Object &obj, AbstractFeature &feature) {
    _index[std::type_index(typeid(feature))].push_back(&obj);
}

void FeatureIndex::on_feature_removing(Object &obj, AbstractFeature &feature) {
    auto ti = std::type_index(typeid(feature));
    auto map_it = _index.find(ti);
    if (map_it == _index.end()) return;
    auto &vec = map_it->second;
    vec.erase(std::remove(vec.begin(), vec.end(), &obj), vec.end());
    if (vec.empty()) _index.erase(map_it);
}

void FeatureIndex::on_child_added(Object & /*parent*/, Object &child) {
    track(child);
}

void FeatureIndex::on_child_removing(Object & /*parent*/, Object &child) {
    untrack(child);
}

} // namespace balsa::scene_graph
