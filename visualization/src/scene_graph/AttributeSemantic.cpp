#include "balsa/scene_graph/AttributeSemantic.hpp"

namespace balsa::scene_graph {

namespace {
    auto vector_of(uint8_t min_components, uint8_t max_components) {
        return [=](const MeshAttribute &attribute) {
            return attribute.is_vector()
                   && attribute.component_count() >= min_components
                   && attribute.component_count() <= max_components;
        };
    }

    // Built-ins are added in declaration order.
    struct Defaults {
        static constexpr auto vertex = AttributeDomain::Vertex;
        catalog::Catalog<AttributeSemanticDefinition> catalog{"attribute semantic"};
        AttributeSemantic position =
            catalog.add({"balsa.position", "Position", vertex, vector_of(2, 3)});
        AttributeSemantic normal =
            catalog.add({"balsa.normal", "Normal", vertex, vector_of(3, 3)});
        // glTF stores the bitangent sign as a fourth component.
        AttributeSemantic tangent =
            catalog.add({"balsa.tangent", "Tangent", vertex, vector_of(3, 4)});
        AttributeSemantic texture_coordinates = catalog.add(
            {"balsa.texture_coordinates", "Texture coordinates", vertex,
             vector_of(2, 2)});
        AttributeSemantic color =
            catalog.add({"balsa.color", "Color", vertex, vector_of(3, 4)});
    };

    auto defaults() -> Defaults & {
        static Defaults d;
        return d;
    }
} // namespace

auto attribute_semantics() -> catalog::Catalog<AttributeSemanticDefinition> & { return defaults().catalog; }

namespace semantics {
    auto position() -> AttributeSemantic { return defaults().position; }
    auto normal() -> AttributeSemantic { return defaults().normal; }
    auto tangent() -> AttributeSemantic { return defaults().tangent; }
    auto texture_coordinates() -> AttributeSemantic {
        return defaults().texture_coordinates;
    }
    auto color() -> AttributeSemantic { return defaults().color; }
} // namespace semantics

} // namespace balsa::scene_graph
