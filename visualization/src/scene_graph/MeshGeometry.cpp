#include "balsa/scene_graph/MeshGeometry.hpp"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <array>
#include <optional>
#include <ranges>

namespace balsa::scene_graph {

MeshGeometry::MeshGeometry() = default;

auto MeshGeometry::operator=(MeshGeometry &&other) noexcept -> MeshGeometry & {
    if (&other == this) return *this;
    _attributes = std::move(other._attributes);
    _explicit_edges = other._explicit_edges;
    _version = std::max(_version, other._version) + 1;
    return *this;
}

auto MeshGeometry::add_user_attribute(MeshAttribute attr) -> MeshAttributeHandle {
    // Edge/face attributes should have one value per edge/face.
    const auto expected = [&]() -> std::optional<std::size_t> {
        switch (attr.domain) {
        case AttributeDomain::Edge: return edge_count();
        case AttributeDomain::Face: return triangle_count();
        case AttributeDomain::Vertex: return std::nullopt;
        }
        return std::nullopt;
    }();
    if (expected && *expected > 0 && attr.count() != *expected) {
        spdlog::warn("MeshGeometry: attribute '{}' has {} elements, but the "
                     "mesh has {}",
                     attr.name,
                     attr.count(),
                     *expected);
    }
    return insert_attribute(std::move(attr));
}

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
    if (is_reserved_name(name)) {
        spdlog::warn("MeshGeometry: attribute '{}' is reserved", name);
        return false;
    }
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

auto MeshGeometry::derive_edges() -> void {
    const auto triangles = triangle_indices();
    std::vector<std::array<uint32_t, 2>> edges;
    edges.reserve(3 * triangles.extent(1));
    for (std::size_t t = 0; t < triangles.extent(1); ++t) {
        for (std::size_t k = 0; k < 3; ++k) {
            const uint32_t a = triangles[k, t];
            const uint32_t b = triangles[(k + 1) % 3, t];
            edges.push_back({std::min(a, b), std::max(a, b)});
        }
    }
    std::ranges::sort(edges);
    auto [first, last] = std::ranges::unique(edges);
    edges.erase(first, last);

    // Contiguous std::array<uint32_t, 2> pairs are a 2 x E column-major
    // buffer.
    set_topology<2>(edge_vertices_name,
                    AttributeDomain::Edge,
                    element_indices_view<2>(
                        edges.empty() ? nullptr : edges.front().data(),
                        edges.size()));
}

auto MeshGeometry::clear() -> void {
    _attributes.clear();
    _explicit_edges = false;
    ++_version;
}

auto MeshGeometry::clone() const -> MeshGeometry {
    MeshGeometry copy;
    copy._attributes.reserve(_attributes.size());
    for (const auto &a : _attributes) {
        copy._attributes.push_back(std::make_shared<MeshAttribute>(*a));
    }
    copy._explicit_edges = _explicit_edges;
    return copy;
}

} // namespace balsa::scene_graph
