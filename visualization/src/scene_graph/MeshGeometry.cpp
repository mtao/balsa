#include "balsa/scene_graph/MeshGeometry.hpp"

#include <algorithm>
#include <array>
#include <ranges>

namespace balsa::scene_graph {

MeshGeometry::MeshGeometry() = default;

auto MeshGeometry::insert_attribute(MeshAttribute attr) -> MeshAttributeHandle {
    auto it = std::ranges::find_if(
        _attributes, [&](const auto &a) { return a->name == attr.name; });
    MeshAttributeHandle handle;
    if (it != _attributes.end()) {
        // Replace in place: existing handles keep referring to it.
        **it = std::move(attr);
        handle = MeshAttributeHandle(*it);
    } else {
        handle = MeshAttributeHandle(_attributes.emplace_back(
            std::make_shared<MeshAttribute>(std::move(attr))));
    }
    ++_version;
    return handle;
}

auto MeshGeometry::remove_attribute(std::string_view name) -> bool {
    const auto removed = std::erase_if(
        _attributes, [&](const auto &a) { return a->name == name; });
    if (removed == 0) return false;
    ++_version;
    return true;
}

auto MeshGeometry::find_attribute(std::string_view name) const
    -> MeshAttributeHandle {
    auto it = std::ranges::find_if(
        _attributes, [&](const auto &a) { return a->name == name; });
    return it == _attributes.end() ? MeshAttributeHandle{}
                                   : MeshAttributeHandle(*it);
}

auto MeshGeometry::owns(const MeshAttributeHandle &attr) const -> bool {
    return attr && std::ranges::any_of(_attributes, [&](const auto &a) {
               return a.get() == attr._attr.get();
           });
}

// ── Topology ────────────────────────────────────────────────────────

auto MeshGeometry::assign_triangles(std::vector<uint32_t> flat) -> void {
    _tri_indices = std::move(flat);
    if (!_explicit_edges) derive_edges();
    ++_version;
}

auto MeshGeometry::assign_edges(std::vector<uint32_t> flat) -> void {
    _edge_indices = std::move(flat);
    _explicit_edges = true;
    ++_version;
}

auto MeshGeometry::derive_edges() -> void {
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

auto MeshGeometry::clear() -> void {
    _attributes.clear();
    _tri_indices.clear();
    _edge_indices.clear();
    _explicit_edges = false;
    ++_version;
}

auto MeshGeometry::copy_from(const MeshGeometry &other) -> void {
    if (&other == this) return;
    _attributes.clear();
    _attributes.reserve(other._attributes.size());
    for (const auto &a : other._attributes) {
        _attributes.push_back(std::make_shared<MeshAttribute>(*a));
    }
    _tri_indices = other._tri_indices;
    _edge_indices = other._edge_indices;
    _explicit_edges = other._explicit_edges;
    ++_version;
}

} // namespace balsa::scene_graph
