#include "balsa/scene_graph/Object.hpp"

#include <algorithm>

namespace balsa::scene_graph {

// ── Constructor / Destructor ────────────────────────────────────────

Object::Object(std::string name)
  : name(std::move(name)) {
    // AffineTransformf default-constructs to identity — nothing to do.
}

Object::~Object() = default;

// ── Move operations ─────────────────────────────────────────────────

Object::Object(Object &&other) noexcept
  : name(std::move(other.name)), visible(other.visible), _local_transform(other._local_transform), _parent(other._parent), _children(std::move(other._children)), _features(std::move(other._features)) {

    // Fix up children's parent pointers.
    for (auto &child : _children) {
        if (child) child->_parent = this;
    }
    // Fix up features' object pointers.
    for (auto &f : _features) {
        if (f) f->_object = this;
    }
    other._parent = nullptr;
}

Object &Object::operator=(Object &&other) noexcept {
    if (this != &other) {
        name = std::move(other.name);
        visible = other.visible;
        _local_transform = other._local_transform;
        _parent = other._parent;
        _children = std::move(other._children);
        _features = std::move(other._features);

        for (auto &child : _children) {
            if (child) child->_parent = this;
        }
        for (auto &f : _features) {
            if (f) f->_object = this;
        }
        other._parent = nullptr;
    }
    return *this;
}

// ── Transform ───────────────────────────────────────────────────────

AffineTransformf Object::world_transform() const {
    if (_parent) {
        return _parent->world_transform() * _local_transform;
    }
    return _local_transform;
}

// ── Hierarchy ───────────────────────────────────────────────────────

Object &Object::add_child(std::string child_name) {
    auto child = std::make_unique<Object>(std::move(child_name));
    child->_parent = this;
    _children.push_back(std::move(child));
    return *_children.back();
}

Object &Object::add_child(std::unique_ptr<Object> child) {
    if (!child) {
        // Create an empty child rather than storing a null.
        return add_child(std::string{});
    }
    // Detach from previous parent if needed.
    if (child->_parent) {
        auto &siblings = child->_parent->_children;
        auto it = std::find_if(siblings.begin(), siblings.end(), [&](const auto &p) { return p.get() == child.get(); });
        if (it != siblings.end()) {
            // Release ownership without destroying — we're taking it.
            it->release();
            siblings.erase(it);
        }
    }
    child->_parent = this;
    _children.push_back(std::move(child));
    return *_children.back();
}

std::unique_ptr<Object> Object::detach() {
    if (!_parent) return nullptr;

    auto &siblings = _parent->_children;
    auto it = std::find_if(siblings.begin(), siblings.end(), [this](const auto &p) { return p.get() == this; });

    if (it == siblings.end()) return nullptr;

    std::unique_ptr<Object> self = std::move(*it);
    siblings.erase(it);
    _parent = nullptr;
    return self;
}

}// namespace balsa::scene_graph
