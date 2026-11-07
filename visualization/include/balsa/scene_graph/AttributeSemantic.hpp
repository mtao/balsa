#pragma once
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "MeshAttribute.hpp"
#include <balsa/catalog/Catalog.hpp>

namespace balsa::scene_graph {

// What a MeshGeometry attribute means (position, normal, a metric tensor,
// ...).  Extensible via attribute_semantics().
struct AttributeSemanticDefinition {
    std::string id;
    std::string label;
    AttributeDomain domain = AttributeDomain::Vertex;
    // Shape / type check for attributes in `domain`; null accepts any.
    std::function<bool(const MeshAttribute &)> accepts;

    [[nodiscard]] auto admits(const MeshAttribute &attribute) const -> bool {
        return attribute.domain == domain && (!accepts || accepts(attribute));
    }
};

using AttributeSemantic = catalog::Entry<AttributeSemanticDefinition>;

// Default catalog, with the built-ins below.
auto attribute_semantics() -> catalog::Catalog<AttributeSemanticDefinition> &;

// Built-in semantics.
namespace semantics {
    auto position() -> AttributeSemantic;
    auto normal() -> AttributeSemantic;
    auto tangent() -> AttributeSemantic;
    auto texture_coordinates() -> AttributeSemantic;
    auto color() -> AttributeSemantic;
} // namespace semantics

} // namespace balsa::scene_graph
