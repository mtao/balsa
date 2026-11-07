#include "balsa/scene_graph/MeshGeometry.hpp"
#include "balsa/scene_graph/AttributeSemantic.hpp"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <array>
#include <optional>
#include <ranges>
#include <utility>

namespace balsa::scene_graph {

MeshGeometry::MeshGeometry() = default;

auto MeshGeometry::operator=(MeshGeometry &&other) noexcept -> MeshGeometry & {
    if (&other == this) return *this;
    _attributes = std::move(other._attributes);
    _semantics = std::move(other._semantics);
    _version = std::max(_version, other._version) + 1;
    return *this;
}

auto MeshGeometry::add_user_attribute(MeshAttribute attr,
                                      const AttributeOptions &options)
    -> MeshAttributeHandle {
    // Simplex attributes should have one value per simplex.
    const auto expected =
        attr.domain == AttributeDomain::Vertex
            ? std::nullopt
            : std::optional(simplex_count(
                  static_cast<std::size_t>(std::to_underlying(attr.domain))));
    if (expected && *expected > 0 && attr.count() != *expected) {
        spdlog::warn("MeshGeometry: attribute '{}' has {} elements, but the "
                     "mesh has {}",
                     attr.name,
                     attr.count(),
                     *expected);
    }
    auto handle = insert_attribute(std::move(attr));
    if (options.semantic && !set_semantic(*options.semantic, handle, options.set)) {
        spdlog::warn("MeshGeometry: attribute '{}' cannot carry semantic '{}' "
                     "(set {})",
                     handle->name,
                     options.semantic->id(),
                     options.set);
    }
    return handle;
}

auto MeshGeometry::insert_attribute(MeshAttribute attr) -> MeshAttributeHandle {
    auto it = std::ranges::find_if(
        _attributes, [&](const auto &a) { return a->name == attr.name; });
    MeshAttributeHandle handle;
    if (it != _attributes.end()) {
        // Replace in place: existing handles keep referring to it.
        **it = std::move(attr);
        handle = MeshAttributeHandle(*it);
        // Drop semantics the new values no longer fit.
        std::erase_if(_semantics, [&](const SemanticEntry &entry) {
            if (entry.attribute._attr != *it) return false;
            if (entry.semantic.value().admits(**it)) return false;
            spdlog::warn("MeshGeometry: attribute '{}' no longer fits semantic "
                         "'{}'; untagged",
                         (*it)->name,
                         entry.semantic.id());
            return true;
        });
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
    const auto it = std::ranges::find_if(
        _attributes, [&](const auto &a) { return a->name == name; });
    if (it == _attributes.end()) return false;
    std::erase_if(_semantics, [&](const SemanticEntry &entry) {
        return entry.attribute._attr == *it;
    });
    _attributes.erase(it);
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

// ── Semantics ───────────────────────────────────────────────────────

auto MeshGeometry::set_semantic(AttributeSemantic semantic,
                                const MeshAttributeHandle &attribute,
                                uint8_t set) -> bool {
    if (!owns(attribute) || !semantic.value().admits(*attribute)) {
        return false;
    }
    const auto it = std::ranges::find_if(_semantics, [&](const auto &entry) {
        return entry.semantic == semantic && entry.set == set;
    });
    if (it == _semantics.end()) {
        _semantics.push_back({semantic, set, attribute});
    } else {
        it->attribute = attribute;
    }
    ++_version;
    return true;
}

auto MeshGeometry::clear_semantic(AttributeSemantic semantic, uint8_t set)
    -> bool {
    const auto removed =
        std::erase_if(_semantics, [&](const SemanticEntry &entry) {
            return entry.semantic == semantic && entry.set == set;
        });
    if (removed == 0) return false;
    ++_version;
    return true;
}

auto MeshGeometry::attribute_for(AttributeSemantic semantic, uint8_t set) const
    -> MeshAttributeHandle {
    const auto it = std::ranges::find_if(_semantics, [&](const auto &entry) {
        return entry.semantic == semantic && entry.set == set;
    });
    return it == _semantics.end() ? MeshAttributeHandle{} : it->attribute;
}

// ── Topology ────────────────────────────────────────────────────────

auto MeshGeometry::simplex_count(std::size_t d) const -> std::size_t {
    switch (d) {
    case 1: return simplices<1>().extent(1);
    case 2: return simplices<2>().extent(1);
    case 3: return simplices<3>().extent(1);
    default: return 0;
    }
}

auto MeshGeometry::dimension() const -> std::size_t {
    for (std::size_t d = max_dimension; d > 0; --d) {
        if (has_simplices(d)) return d;
    }
    return 0;
}

auto MeshGeometry::clear() -> void {
    _attributes.clear();
    _semantics.clear();
    ++_version;
}

auto MeshGeometry::clone() const -> MeshGeometry {
    MeshGeometry copy;
    copy._attributes.reserve(_attributes.size());
    for (const auto &a : _attributes) {
        copy._attributes.push_back(std::make_shared<MeshAttribute>(*a));
    }
    // Point the entries at the copies (clone keeps the attribute order).
    for (const auto &entry : _semantics) {
        const auto index = std::ranges::find(_attributes, entry.attribute._attr)
                           - _attributes.begin();
        copy._semantics.push_back(
            {entry.semantic, entry.set,
             MeshAttributeHandle(copy._attributes[static_cast<std::size_t>(index)])});
    }
    return copy;
}

auto MeshGeometry::share_vertex_attributes() const -> MeshGeometry {
    MeshGeometry view;
    for (const auto &a : _attributes) {
        if (a->domain == AttributeDomain::Vertex) view._attributes.push_back(a);
    }
    for (const auto &entry : _semantics) {
        if (entry.attribute->domain == AttributeDomain::Vertex) {
            view._semantics.push_back(entry);
        }
    }
    return view;
}

} // namespace balsa::scene_graph
