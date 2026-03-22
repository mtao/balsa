#include "balsa/scene_graph/Object.hpp"

#include <algorithm>
#include <cmath>

#include <zipper/transform/quaternion_transform.hpp>

namespace balsa::scene_graph {

namespace {

    // ── Helpers for TRS decomposition ──────────────────────────────

    // Column norm of a 3×3 linear block (given a slice view or matrix).
    // We compute it manually since the linear() slice may not have .norm().
    template<typename M>
    float col_norm(const M &m, int col) {
        float sum = 0;
        for (int r = 0; r < 3; ++r) {
            float v = static_cast<float>(m(r, col));
            sum += v * v;
        }
        return std::sqrt(sum);
    }

    // 3×3 determinant from element access.
    template<typename M>
    float det3(const M &m) {
        return static_cast<float>(m(0, 0)) * (static_cast<float>(m(1, 1)) * static_cast<float>(m(2, 2)) - static_cast<float>(m(1, 2)) * static_cast<float>(m(2, 1))) - static_cast<float>(m(0, 1)) * (static_cast<float>(m(1, 0)) * static_cast<float>(m(2, 2)) - static_cast<float>(m(1, 2)) * static_cast<float>(m(2, 0))) + static_cast<float>(m(0, 2)) * (static_cast<float>(m(1, 0)) * static_cast<float>(m(2, 1)) - static_cast<float>(m(1, 1)) * static_cast<float>(m(2, 0)));
    }

    constexpr float k_deg2rad = std::numbers::pi_v<float> / 180.0f;
    constexpr float k_rad2deg = 180.0f / std::numbers::pi_v<float>;

}// namespace

// ── Constructor / Destructor ────────────────────────────────────────

Object::Object(std::string name)
  : name(std::move(name)) {
    // _translation: Vec3f default-constructs to zero.
    // _rotation: explicitly initialized to identity quaternion {1,0,0,0}.
    // _scale: needs explicit init to {1,1,1}.
    _scale(0) = 1.0f;
    _scale(1) = 1.0f;
    _scale(2) = 1.0f;
}

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
    auto rad = ::zipper::transform::euler_angles(_rotation);
    Vec3f deg;
    deg(0) = static_cast<float>(rad(0)) * k_rad2deg;
    deg(1) = static_cast<float>(rad(1)) * k_rad2deg;
    deg(2) = static_cast<float>(rad(2)) * k_rad2deg;
    return deg;
}

void Object::set_rotation_euler(const Vec3f &degrees) {
    // Convert degrees to radians.
    float pitch = degrees(0) * k_deg2rad;// X
    float yaw = degrees(1) * k_deg2rad;// Y
    float roll = degrees(2) * k_deg2rad;// Z

    // Tait-Bryan ZYX intrinsic: q = qZ * qY * qX
    // (matches euler_angles() convention in zipper)
    float cx = std::cos(pitch * 0.5f), sx = std::sin(pitch * 0.5f);
    float cy = std::cos(yaw * 0.5f), sy = std::sin(yaw * 0.5f);
    float cz = std::cos(roll * 0.5f), sz = std::sin(roll * 0.5f);

    _rotation = Quaternionf(
      cx * cy * cz + sx * sy * sz,// w
      sx * cy * cz - cx * sy * sz,// x
      cx * sy * cz + sx * cy * sz,// y
      cx * cy * sz - sx * sy * cz);// z
}

// ── Convenience mutators ────────────────────────────────────────────

void Object::translate(const Vec3f &delta) {
    _translation(0) = static_cast<float>(_translation(0)) + static_cast<float>(delta(0));
    _translation(1) = static_cast<float>(_translation(1)) + static_cast<float>(delta(1));
    _translation(2) = static_cast<float>(_translation(2)) + static_cast<float>(delta(2));
}

void Object::rotate(const Quaternionf &q) {
    // Post-multiply: new_rotation = q * old_rotation
    _rotation = (q * _rotation).eval();
}

void Object::set_uniform_scale(float s) {
    _scale(0) = s;
    _scale(1) = s;
    _scale(2) = s;
}

void Object::reset_transform() {
    _translation(0) = 0;
    _translation(1) = 0;
    _translation(2) = 0;
    _rotation = Quaternionf(1, 0, 0, 0);
    _scale(0) = 1;
    _scale(1) = 1;
    _scale(2) = 1;
}

// ── Composed transforms ─────────────────────────────────────────────

AffineTransformf Object::local_transform() const {
    // Build T * R * S manually for efficiency.
    // The resulting 4×4 column-major matrix is:
    //   [ R*S | t ]
    //   [ 0   | 1 ]
    //
    // where R is the 3×3 rotation matrix from the quaternion and
    // S is a diagonal matrix of scale factors.

    auto rot_mat = ::zipper::transform::to_rotation_matrix(_rotation);

    AffineTransformf xf;// identity
    // Fill the linear block (upper-left 3×3) = R * diag(scale)
    for (int c = 0; c < 3; ++c) {
        float sc = static_cast<float>(_scale(c));
        for (int r = 0; r < 3; ++r) {
            xf(r, c) = static_cast<float>(rot_mat(r, c)) * sc;
        }
    }
    // Fill the translation column
    xf(0, 3) = static_cast<float>(_translation(0));
    xf(1, 3) = static_cast<float>(_translation(1));
    xf(2, 3) = static_cast<float>(_translation(2));

    // Last row is already [0, 0, 0, 1] from identity init.
    return xf;
}

void Object::set_from_transform(const AffineTransformf &xf) {
    // 1. Extract translation from column 3.
    _translation(0) = static_cast<float>(xf(0, 3));
    _translation(1) = static_cast<float>(xf(1, 3));
    _translation(2) = static_cast<float>(xf(2, 3));

    // 2. Extract scale as column norms of the linear block.
    auto lin = xf.linear();
    float sx = col_norm(lin, 0);
    float sy = col_norm(lin, 1);
    float sz = col_norm(lin, 2);

    // Handle reflection: if the determinant is negative, negate one scale axis.
    if (det3(lin) < 0) {
        sx = -sx;
    }

    _scale(0) = sx;
    _scale(1) = sy;
    _scale(2) = sz;

    // 3. Build the pure rotation matrix by dividing out the scale.
    //    Avoid division by zero for degenerate scale.
    ::zipper::Matrix<float, 3, 3> rot;
    for (int c = 0; c < 3; ++c) {
        float s = (c == 0) ? sx : (c == 1 ? sy : sz);
        float inv_s = (std::abs(s) > 1e-7f) ? (1.0f / s) : 0.0f;
        for (int r = 0; r < 3; ++r) {
            rot(r, c) = static_cast<float>(lin(r, c)) * inv_s;
        }
    }

    // 4. Convert the rotation matrix to a quaternion.
    _rotation = ::zipper::transform::to_quaternion(rot);
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
