#include "balsa/scene_graph/Object.hpp"

#include <algorithm>

#include <zipper/expression/nullary/Constant.hpp>
#include <zipper/transform/common.hpp>
#include <zipper/transform/decompose.hpp>
#include <zipper/transform/quaternion_transform.hpp>

namespace balsa::scene_graph {

// ── Constructor / Destructor ────────────────────────────────────────

Object::Object(std::string name)
  : name(std::move(name)) {}

Object::~Object() = default;

// ── Move operations ─────────────────────────────────────────────────

Object::Object(Object &&other) noexcept
  : name(std::move(other.name)),
    visible(other.visible),
    selectable(other.selectable),
    _translation(other._translation),
    _rotation(other._rotation),
    _scale(other._scale),
    _parent(other._parent),
    _children(std::move(other._children)),
    _features(std::move(other._features)) {

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
        selectable = other.selectable;
        _translation = other._translation;
        _rotation = other._rotation;
        _scale = other._scale;
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

// ── Euler angles ────────────────────────────────────────────────────

Vec3f Object::rotation_euler() const {
    const Vec3f rad = ::zipper::transform::euler_angles(_rotation);
    return Vec3f(::zipper::transform::degrees(1.0f) * rad);
}

void Object::set_rotation_euler(const Vec3f &degrees) {
    const Vec3f radians(::zipper::transform::radians(1.0f) * degrees);
    _rotation = ::zipper::transform::from_euler_angles(radians);
}

// ── Convenience mutators ────────────────────────────────────────────

void Object::translate(const Vec3f &delta) {
    _translation = (_translation + delta).eval();
}

void Object::rotate(const Quaternionf &q) {
    // Post-multiply: new_rotation = q * old_rotation
    _rotation = (q * _rotation).eval();
}

void Object::set_uniform_scale(float s) {
    _scale = ::zipper::expression::nullary::Constant<float, 3>(s);
}

void Object::reset_transform() {
    _translation = ::zipper::expression::nullary::Constant<float, 3>(0.0f);
    _rotation = Quaternionf(1, 0, 0, 0);
    _scale = ::zipper::expression::nullary::Constant<float, 3>(1.0f);
}

// ── Composed transforms ─────────────────────────────────────────────

AffineTransformf Object::local_transform() const {
    // Compose T * R * S using zipper's transform composition.
    Translation3f t(_translation);
    Rotation3f r(::zipper::transform::to_rotation_matrix(_rotation));
    Scaling3f s(_scale);
    return t * r * s;
}

void Object::set_from_transform(const AffineTransformf &xf) {
    auto [t, r, s] = ::zipper::transform::trs_decompose(xf);

    _translation = t.vector();
    _scale = s.factors();
    _rotation = ::zipper::transform::to_quaternion(r.matrix());
}

AffineTransformf Object::world_transform() const {
    if (_parent) {
        return _parent->world_transform() * local_transform();
    }
    return local_transform();
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
        auto it = std::ranges::find(siblings, child.get(), &std::unique_ptr<Object>::get);
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
    if (permanent) return nullptr;// permanent objects cannot be detached

    auto &siblings = _parent->_children;
    auto it = std::ranges::find(siblings, this, &std::unique_ptr<Object>::get);

    if (it == siblings.end()) return nullptr;

    std::unique_ptr<Object> self = std::move(*it);
    siblings.erase(it);
    _parent = nullptr;
    return self;
}

// ── Feature removal ─────────────────────────────────────────────────

bool Object::remove_feature(AbstractFeature *feature) {
    if (!feature) return false;
    auto it = std::ranges::find(_features, feature, &std::unique_ptr<AbstractFeature>::get);
    if (it == _features.end()) return false;
    _features.erase(it);
    return true;
}

}// namespace balsa::scene_graph
