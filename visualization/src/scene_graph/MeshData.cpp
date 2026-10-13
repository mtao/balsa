#include "balsa/scene_graph/MeshData.hpp"

#include <spdlog/spdlog.h>
#include <zipper/MatrixBase.hpp>
#include <zipper/VectorBase.hpp>

#include <algorithm>
#include <array>
#include <ranges>

namespace balsa::scene_graph {

// ── RoleBinding ─────────────────────────────────────────────────────

auto RoleBinding::clear() -> void {
    _attr = {};
    _component.reset();
    _magnitude.reset();
}

namespace {
    // Per-element magnitudes of attr as a single-component attribute:
    // float stays float, other types become double.
    auto magnitude_of(const MeshAttribute &attr) -> MeshAttribute {
        return attr.visit([&](const auto &values) {
            using V = std::remove_const_t<
                typename std::decay_t<decltype(values)>::element_type>;
            using R = std::conditional_t<std::is_same_v<V, float>, float, double>;
            std::vector<R> norms(values.extent(1));
            typename balsa::zipper::VectorX<R>::span_type out{
                std::span<R>(norms)};
            const auto as_r = ::zipper::MatrixBase(values).template cast<R>();
            out = as_r.colwise().norm();
            return MeshAttribute(
                attr.name + " (magnitude)",
                attr.domain,
                ::zipper::mdspan<const R, ::zipper::dextents<1>>(norms.data(),
                                                                norms.size()));
        });
    }
} // namespace

// ── MeshData ────────────────────────────────────────────────────────

MeshData::MeshData() = default;

auto MeshData::insert_attribute(MeshAttribute attr) -> MeshAttributeHandle {
    auto it = std::ranges::find_if(
        _attributes, [&](const auto &a) { return a->name == attr.name; });
    MeshAttributeHandle handle;
    if (it != _attributes.end()) {
        // Replace in place: existing handles/bindings keep referring to it.
        **it = std::move(attr);
        handle = MeshAttributeHandle(*it);
        refresh_bindings(handle);
    } else {
        handle = MeshAttributeHandle(_attributes.emplace_back(
            std::make_shared<MeshAttribute>(std::move(attr))));
        if (handle->name == "vertex_positions" && !_position.is_bound()) {
            assign_position(handle);
        } else if (handle->name == "vertex_normals" && !_normal.is_bound()) {
            assign_normal(handle);
        }
    }

    ++_version;
    return handle;
}

auto MeshData::remove_attribute(std::string_view name) -> bool {
    auto it = std::ranges::find_if(
        _attributes, [&](const auto &a) { return a->name == name; });
    if (it == _attributes.end()) return false;
    const MeshAttributeHandle removed(*it);
    for (auto *b : {&_position, &_normal, &_scalar}) {
        if (b->attribute() == removed) b->clear();
    }
    if (!_scalar.is_bound()) _scalar_component = -1;
    _attributes.erase(it);
    ++_version;
    return true;
}

auto MeshData::find_attribute(std::string_view name) const
    -> MeshAttributeHandle {
    auto it = std::ranges::find_if(
        _attributes, [&](const auto &a) { return a->name == name; });
    return it == _attributes.end() ? MeshAttributeHandle{}
                                   : MeshAttributeHandle(*it);
}

auto MeshData::owns(const MeshAttributeHandle &attr) const -> bool {
    return attr && std::ranges::any_of(_attributes, [&](const auto &a) {
               return a.get() == attr._attr.get();
           });
}

// ── Topology ────────────────────────────────────────────────────────

auto MeshData::assign_triangles(std::vector<uint32_t> flat) -> void {
    _tri_indices = std::move(flat);
    if (!_explicit_edges) derive_edges();
    ++_version;
}

auto MeshData::assign_edges(std::vector<uint32_t> flat) -> void {
    _edge_indices = std::move(flat);
    _explicit_edges = true;
    ++_version;
}

auto MeshData::derive_edges() -> void {
    std::vector<std::array<uint32_t, 2>> edges;
    edges.reserve(_tri_indices.size());
    for (auto tri : _tri_indices | std::views::chunk(3)) {
        for (std::size_t k = 0; k < 3; ++k) {
            auto a = tri[k], b = tri[(k + 1) % 3];
            edges.push_back({std::min(a, b), std::max(a, b)});
        }
    }
    std::ranges::sort(edges);
    auto [first, last] = std::ranges::unique(edges);
    edges.erase(first, last);

    _edge_indices.clear();
    _edge_indices.reserve(edges.size() * 2);
    for (const auto &[a, b] : edges) {
        _edge_indices.push_back(a);
        _edge_indices.push_back(b);
    }
}

auto MeshData::clear() -> void {
    _attributes.clear();
    _position.clear();
    _normal.clear();
    _scalar.clear();
    _scalar_component = -1;
    _tri_indices.clear();
    _edge_indices.clear();
    _explicit_edges = false;
    ++_version;
}

// ── Role assignment ─────────────────────────────────────────────────

auto MeshData::assign_position(std::string_view name) -> bool {
    auto a = find_attribute(name);
    if (!a) {
        clear_position();
        return false;
    }
    return assign_position(a);
}

auto MeshData::assign_normal(std::string_view name) -> bool {
    auto a = find_attribute(name);
    if (!a) {
        clear_normal();
        return false;
    }
    return assign_normal(a);
}

auto MeshData::assign_scalar(std::string_view name, int component) -> bool {
    auto a = find_attribute(name);
    if (!a) {
        clear_scalar();
        return false;
    }
    return assign_scalar(a, component);
}

auto MeshData::assign_position(const MeshAttributeHandle &attr) -> bool {
    _position = owns(attr) ? bind_vector(attr, 1) : RoleBinding{};
    ++_version;
    return _position.is_bound();
}

auto MeshData::assign_normal(const MeshAttributeHandle &attr) -> bool {
    _normal = owns(attr) ? bind_vector(attr, 2) : RoleBinding{};
    ++_version;
    return _normal.is_bound();
}

auto MeshData::assign_scalar(const MeshAttributeHandle &attr, int component)
    -> bool {
    _scalar_component = component;
    _scalar = owns(attr) ? bind_scalar(attr, component) : RoleBinding{};
    ++_version;
    return _scalar.is_bound();
}

auto MeshData::set_scalar_component(int component) -> bool {
    auto a = _scalar.attribute(); // copy: assign_scalar replaces _scalar
    if (!a) return false;
    return assign_scalar(a, component);
}

auto MeshData::clear_position() -> void {
    _position.clear();
    ++_version;
}

auto MeshData::clear_normal() -> void {
    _normal.clear();
    ++_version;
}

auto MeshData::clear_scalar() -> void {
    _scalar.clear();
    _scalar_component = -1;
    ++_version;
}

auto MeshData::refresh_bindings(const MeshAttributeHandle &attr) -> void {
    if (_position.attribute() == attr) _position = bind_vector(attr, 1);
    if (_normal.attribute() == attr) _normal = bind_vector(attr, 2);
    if (_scalar.attribute() == attr) {
        _scalar = bind_scalar(attr, _scalar_component);
    }
}

// ── Binding builders ────────────────────────────────────────────────

auto MeshData::bind_vector(const MeshAttributeHandle &attr,
                           uint8_t min_components) -> RoleBinding {
    RoleBinding binding;
    if (!attr) return binding;
    if (attr->domain != AttributeDomain::Vertex
        || attr->component_count() < min_components) {
        spdlog::warn(
            "MeshData: attribute '{}' is not a vertex attribute with "
            ">= {} components",
            attr->name,
            min_components);
        return binding;
    }
    binding._attr = attr;
    return binding;
}

auto MeshData::bind_scalar(const MeshAttributeHandle &attr, int component)
    -> RoleBinding {
    RoleBinding binding;
    if (!attr) return binding;
    if (attr->domain != AttributeDomain::Vertex || attr->component_count() == 0) {
        spdlog::warn("MeshData: attribute '{}' is not a vertex attribute",
                     attr->name);
        return binding;
    }
    if (component >= attr->component_count()) {
        spdlog::warn("MeshData: component {} out of range for '{}'",
                     component,
                     attr->name);
        return binding;
    }

    binding._attr = attr;
    if (component >= 0) {
        binding._component = static_cast<uint8_t>(component);
    } else if (attr->component_count() == 1) {
        binding._component = 0;
    } else {
        binding._magnitude =
            std::make_shared<const MeshAttribute>(magnitude_of(*attr));
        binding._component = 0;
    }
    return binding;
}

} // namespace balsa::scene_graph
