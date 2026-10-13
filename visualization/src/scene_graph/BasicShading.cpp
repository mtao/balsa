#include "balsa/scene_graph/BasicShading.hpp"

#include <spdlog/spdlog.h>

#include "balsa/scene_graph/MeshGeometry.hpp"
#include "balsa/scene_graph/Object.hpp"

namespace balsa::scene_graph {

BasicShading::BasicShading() = default;

auto BasicShading::geometry() const -> const MeshGeometry * {
    return is_attached() ? object().find_feature<MeshGeometry>() : nullptr;
}

// ── Resolution ──────────────────────────────────────────────────────

auto BasicShading::sync() const -> void {
    const auto *g = geometry();
    const uint64_t v = g ? g->version() : 0;
    if (g == _synced_geometry && v == _synced_geometry_version) return;
    _synced_geometry = g;
    _synced_geometry_version = v;
    resolve(_position, Kind::Position);
    resolve(_normal, Kind::Normal);
    resolve(_scalar, Kind::Scalar);
}

auto BasicShading::default_attribute_name(Kind kind) -> std::string_view {
    switch (kind) {
    case Kind::Position: return "vertex_positions";
    case Kind::Normal: return "vertex_normals";
    case Kind::Scalar: return {};
    }
    return {};
}

auto BasicShading::compatible(Kind kind, const MeshAttribute &attr,
                              int component, bool warn) -> bool {
    if (attr.domain != AttributeDomain::Vertex) {
        if (warn) {
            spdlog::warn("BasicShading: attribute '{}' is not a vertex attribute",
                         attr.name);
        }
        return false;
    }
    const uint8_t needed = kind == Kind::Normal ? 2 : 1;
    if (attr.component_count() < needed) {
        if (warn) {
            spdlog::warn("BasicShading: attribute '{}' has {} components, "
                         "needs >= {}",
                         attr.name,
                         attr.component_count(),
                         needed);
        }
        return false;
    }
    if (kind == Kind::Scalar && component >= attr.component_count()) {
        if (warn) {
            spdlog::warn("BasicShading: component {} out of range for '{}'",
                         component,
                         attr.name);
        }
        return false;
    }
    return true;
}

auto BasicShading::resolve(Role &role, Kind kind) const -> void {
    role.binding.clear();
    const auto *g = _synced_geometry;
    if (g == nullptr) return;
    if (role.attribute && !g->owns(role.attribute)) role.attribute = {};
    if (!role.attribute && !role.user_set) {
        if (const auto name = default_attribute_name(kind); !name.empty()) {
            role.attribute = g->find_attribute(name);
        }
    }
    if (!role.attribute
        || !compatible(kind, *role.attribute, role.component, /*warn=*/false)) {
        return;
    }

    if (kind != Kind::Scalar) {
        role.binding = RoleBinding::whole(role.attribute);
    } else if (role.component >= 0) {
        role.binding = RoleBinding::component(
            role.attribute, static_cast<uint8_t>(role.component));
    } else if (role.attribute->component_count() == 1) {
        role.binding = RoleBinding::component(role.attribute, 0);
    } else {
        role.binding = RoleBinding::magnitude(role.attribute);
    }
}

// ── Role assignment ─────────────────────────────────────────────────

auto BasicShading::find(std::string_view name) const -> MeshAttributeHandle {
    const auto *g = geometry();
    return g ? g->find_attribute(name) : MeshAttributeHandle{};
}

auto BasicShading::assign(Role &role, Kind kind,
                          const MeshAttributeHandle &attr, int component)
    -> bool {
    sync();
    role.user_set = true;
    role.component = component;
    role.attribute = {};
    const auto *g = _synced_geometry;
    if (g != nullptr && g->owns(attr)
        && compatible(kind, *attr, component, /*warn=*/true)) {
        role.attribute = attr;
    }
    resolve(role, kind);
    ++_version;
    return role.binding.is_bound();
}

auto BasicShading::clear(Role &role) -> void {
    role.user_set = true;
    role.attribute = {};
    role.component = -1;
    role.binding.clear();
    ++_version;
}

auto BasicShading::assign_position(std::string_view name) -> bool {
    auto a = find(name);
    if (!a) {
        clear_position();
        return false;
    }
    return assign_position(a);
}

auto BasicShading::assign_normal(std::string_view name) -> bool {
    auto a = find(name);
    if (!a) {
        clear_normal();
        return false;
    }
    return assign_normal(a);
}

auto BasicShading::assign_scalar(std::string_view name, int component)
    -> bool {
    auto a = find(name);
    if (!a) {
        clear_scalar();
        return false;
    }
    return assign_scalar(a, component);
}

auto BasicShading::assign_position(const MeshAttributeHandle &attr) -> bool {
    return assign(_position, Kind::Position, attr, -1);
}

auto BasicShading::assign_normal(const MeshAttributeHandle &attr) -> bool {
    return assign(_normal, Kind::Normal, attr, -1);
}

auto BasicShading::assign_scalar(const MeshAttributeHandle &attr,
                                 int component) -> bool {
    return assign(_scalar, Kind::Scalar, attr, component);
}

auto BasicShading::set_scalar_component(int component) -> bool {
    sync();
    auto a = _scalar.attribute; // copy: assign() resets the role
    if (!a) return false;
    return assign_scalar(a, component);
}

auto BasicShading::clear_position() -> void { clear(_position); }
auto BasicShading::clear_normal() -> void { clear(_normal); }
auto BasicShading::clear_scalar() -> void { clear(_scalar); }

auto BasicShading::copy_from(const BasicShading &other) -> void {
    if (&other == this) return;
    other.sync();
    sync();
    auto copy_role = [this](Role &dst, const Role &src, Kind kind) {
        dst.user_set = src.user_set;
        dst.component = src.component;
        dst.attribute =
            src.user_set && src.attribute ? find(src.attribute->name)
                                          : MeshAttributeHandle{};
        resolve(dst, kind);
    };
    copy_role(_position, other._position, Kind::Position);
    copy_role(_normal, other._normal, Kind::Normal);
    copy_role(_scalar, other._scalar, Kind::Scalar);
    _render_state = other._render_state;
    ++_version;
}

auto BasicShading::position_binding() const -> const RoleBinding & {
    sync();
    return _position.binding;
}
auto BasicShading::normal_binding() const -> const RoleBinding & {
    sync();
    return _normal.binding;
}
auto BasicShading::scalar_binding() const -> const RoleBinding & {
    sync();
    return _scalar.binding;
}

} // namespace balsa::scene_graph
