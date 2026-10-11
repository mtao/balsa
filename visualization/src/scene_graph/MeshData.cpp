#include "balsa/scene_graph/MeshData.hpp"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <numeric>
#include <ranges>

namespace balsa::scene_graph {

// ── RoleBinding ─────────────────────────────────────────────────────

auto RoleBinding::clear() -> void {
    source.clear();
    component_count = 0;
    _data = nullptr;
    _count = 0;
    _extracted.clear();
}

// ── MeshData ────────────────────────────────────────────────────────

MeshData::MeshData() = default;

auto MeshData::add_attribute(std::string name,
                             AttributeDomain domain,
                             uint8_t component_count,
                             std::span<const float> data)
    -> const MeshAttribute & {
    return insert_attribute(MeshAttribute{std::move(name),
                                          domain,
                                          component_count,
                                          {data.begin(), data.end()}});
}

auto MeshData::add_attribute(std::string name,
                             AttributeDomain domain,
                             uint8_t component_count,
                             std::span<const double> data)
    -> const MeshAttribute & {
    MeshAttribute attr{std::move(name), domain, component_count, {}};
    attr.data.assign_range(
        data | std::views::transform([](double v) {
            return static_cast<float>(v);
        }));
    return insert_attribute(std::move(attr));
}

auto MeshData::insert_attribute(MeshAttribute attr) -> const MeshAttribute & {
    if (attr.component_count == 0
        || attr.data.size() % attr.component_count != 0) {
        spdlog::warn("MeshData: attribute '{}' has {} values, not a multiple "
                     "of {} components",
                     attr.name,
                     attr.data.size(),
                     attr.component_count);
    }

    auto it = std::ranges::find(_attributes, attr.name, &MeshAttribute::name);
    MeshAttribute *slot = nullptr;
    if (it != _attributes.end()) {
        *it = std::move(attr);
        slot = &*it;
    } else {
        slot = &_attributes.emplace_back(std::move(attr));
    }
    const std::string name = slot->name;

    // Storage may have moved; refresh existing bindings first.
    rebind_all();

    if (name == "vertex_positions" && !_position.is_bound()) {
        assign_position(name);
    } else if (name == "vertex_normals" && !_normal.is_bound()) {
        assign_normal(name);
    }

    ++_version;
    return *find_attribute(name);
}

auto MeshData::remove_attribute(std::string_view name) -> bool {
    auto it = std::ranges::find(_attributes, name, &MeshAttribute::name);
    if (it == _attributes.end()) return false;
    _attributes.erase(it);
    rebind_all();
    ++_version;
    return true;
}

auto MeshData::find_attribute(std::string_view name) const
    -> const MeshAttribute * {
    auto it = std::ranges::find(_attributes, name, &MeshAttribute::name);
    return it == _attributes.end() ? nullptr : &*it;
}

// ── Topology ────────────────────────────────────────────────────────

auto MeshData::set_triangle_indices(std::span<const uint32_t> indices)
    -> void {
    _tri_indices.assign(indices.begin(), indices.end());
    if (!_explicit_edges) derive_edges();
    ++_version;
}

auto MeshData::set_edge_indices(std::span<const uint32_t> indices) -> void {
    _edge_indices.assign(indices.begin(), indices.end());
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
    _position = bind_vector(find_attribute(name), 1);
    ++_version;
    return _position.is_bound();
}

auto MeshData::assign_normal(std::string_view name) -> bool {
    _normal = bind_vector(find_attribute(name), 2);
    ++_version;
    return _normal.is_bound();
}

auto MeshData::assign_scalar(std::string_view name, int component) -> bool {
    _scalar_component = component;
    _scalar = bind_scalar(find_attribute(name), component);
    ++_version;
    return _scalar.is_bound();
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

auto MeshData::rebind_all() -> void {
    if (!_position.source.empty()) {
        _position = bind_vector(find_attribute(_position.source), 1);
    }
    if (!_normal.source.empty()) {
        _normal = bind_vector(find_attribute(_normal.source), 2);
    }
    if (!_scalar.source.empty()) {
        _scalar = bind_scalar(find_attribute(_scalar.source), _scalar_component);
    }
}

// ── Binding builders ────────────────────────────────────────────────

auto MeshData::bind_vector(const MeshAttribute *attr, uint8_t min_components)
    -> RoleBinding {
    RoleBinding binding;
    if (attr == nullptr) return binding;
    if (attr->domain != AttributeDomain::Vertex
        || attr->component_count < min_components) {
        spdlog::warn("MeshData: attribute '{}' is not a vertex attribute with "
                     ">= {} components",
                     attr->name,
                     min_components);
        return binding;
    }
    binding.source = attr->name;
    binding.component_count = attr->component_count;
    binding._data = attr->data.data();
    binding._count = attr->count();
    return binding;
}

auto MeshData::bind_scalar(const MeshAttribute *attr, int component)
    -> RoleBinding {
    RoleBinding binding;
    if (attr == nullptr) return binding;
    if (attr->domain != AttributeDomain::Vertex
        || attr->component_count == 0) {
        spdlog::warn("MeshData: attribute '{}' is not a vertex attribute",
                     attr->name);
        return binding;
    }
    if (component >= attr->component_count) {
        spdlog::warn("MeshData: component {} out of range for '{}'",
                     component,
                     attr->name);
        return binding;
    }

    binding.source = attr->name;
    binding.component_count = 1;
    binding._count = attr->count();

    if (attr->component_count == 1) {
        binding._data = attr->data.data();
        return binding;
    }

    auto elements = attr->data | std::views::chunk(attr->component_count);
    if (component >= 0) {
        binding._extracted.assign_range(
            elements | std::views::transform([component](auto e) {
                return e[static_cast<std::size_t>(component)];
            }));
    } else {
        binding._extracted.assign_range(
            elements | std::views::transform([](auto e) {
                return std::sqrt(
                    std::inner_product(e.begin(), e.end(), e.begin(), 0.0f));
            }));
    }
    binding._data = binding._extracted.data();
    return binding;
}

} // namespace balsa::scene_graph
