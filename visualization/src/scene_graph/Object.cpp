#include "balsa/scene_graph/Object.hpp"

#include <algorithm>

#include <zipper/expression/nullary/Constant.hpp>
#include <zipper/transform/common.hpp>
#include <zipper/transform/decompose.hpp>
#include <zipper/transform/quaternion_transform.hpp>

namespace balsa::scene_graph {

// ── Constructor / Destructor ────────────────────────────────────────

Object::Object(std::string name) : name(std::move(name)) {}

Object::~Object() { destroying.emit(*this); }

// ── Euler angles ────────────────────────────────────────────────────

auto Object::rotation_euler() const -> Vec3f {
    const Vec3f rad = ::zipper::transform::euler_angles(_rotation);
    return Vec3f(::zipper::transform::degrees(1.0f) * rad);
}

auto Object::set_rotation_euler(const Vec3f &degrees) -> void {
    const Vec3f radians(::zipper::transform::radians(1.0f) * degrees);
    _rotation = ::zipper::transform::from_euler_angles(radians);
}

// ── Convenience mutators ────────────────────────────────────────────

auto Object::translate(const Vec3f &delta) -> void {
    _translation = (_translation + delta).eval();
}

auto Object::rotate(const Quaternionf &q) -> void {
    // Post-multiply: new_rotation = q * old_rotation
    _rotation = (q * _rotation).eval();
}

auto Object::set_uniform_scale(float s) -> void {
    _scale = ::zipper::expression::nullary::Constant<float, 3>(s);
}

auto Object::reset_transform() -> void {
    _translation = ::zipper::expression::nullary::Constant<float, 3>(0.0f);
    _rotation = Quaternionf(1, 0, 0, 0);
    _scale = ::zipper::expression::nullary::Constant<float, 3>(1.0f);
}

// ── Composed transforms ─────────────────────────────────────────────

auto Object::local_transform() const -> AffineTransformf {
    // Compose T * R * S using zipper's transform composition.
    Translation3f t(_translation);
    Rotation3f r(::zipper::transform::to_rotation_matrix(_rotation));
    Scaling3f s(_scale);
    return t * r * s;
}

auto Object::set_from_transform(const AffineTransformf &xf) -> void {
    auto [t, r, s] = ::zipper::transform::trs_decompose(xf);

    _translation = t.vector();
    _scale = s.factors();
    _rotation = ::zipper::transform::to_quaternion(r.matrix());
}

auto Object::world_transform() const -> AffineTransformf {
    if (_parent) {
        return _parent->world_transform() * local_transform();
    }
    return local_transform();
}

// ── Hierarchy ───────────────────────────────────────────────────────

auto Object::add_child(std::string child_name) -> Object & {
    return add_child(std::make_unique<Object>(std::move(child_name)));
}

auto Object::release_child(Object &child) -> std::unique_ptr<Object> {
    auto it = std::ranges::find(_children, &child, &std::unique_ptr<Object>::get);
    if (it == _children.end()) return nullptr;
    child_removing.emit(*this, child);
    std::unique_ptr<Object> owned = std::move(*it);
    _children.erase(it);
    owned->_parent = nullptr;
    return owned;
}

auto Object::add_child(std::unique_ptr<Object> child) -> Object & {
    if (!child) {
        // Create an empty child rather than storing a null.
        child = std::make_unique<Object>();
    }
    // A child still listed under its previous parent is owned twice:
    // drop that owner without destroying the object.
    if (child->_parent != nullptr) {
        static_cast<void>(child->_parent->release_child(*child).release());
    }
    child->_parent = this;
    Object &added = *_children.emplace_back(std::move(child));
    child_added.emit(*this, added);
    return added;
}

auto Object::detach() -> std::unique_ptr<Object> {
    if (_parent == nullptr || permanent) return nullptr;
    return _parent->release_child(*this);
}

auto Object::is_descendant_of(const Object &ancestor) const -> bool {
    bool found = false;
    for_each_ancestor([&](const Object &a) {
        found = &a == &ancestor;
        return !found;
    });
    return found;
}

// ── Feature removal ─────────────────────────────────────────────────

auto Object::remove_feature(AbstractFeature *feature) -> bool {
    if (feature == nullptr) return false;
    auto it = std::ranges::find(
        _features, feature, &std::unique_ptr<AbstractFeature>::get);
    if (it == _features.end()) return false;
    feature_removing.emit(*this, *feature);
    _features.erase(it);
    return true;
}

} // namespace balsa::scene_graph
