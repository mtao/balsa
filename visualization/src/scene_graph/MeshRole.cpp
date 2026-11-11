#include "balsa/scene_graph/MeshRole.hpp"

#include <array>

namespace balsa::scene_graph {

namespace {
    // Built-ins are added in declaration order.
    struct Defaults {
        catalog::Catalog<MeshRoleDefinition> catalog{"mesh role"};
        MeshRole position = catalog.add({"balsa.position", "Position"});
        MeshRole normal = catalog.add({"balsa.normal", "Normal"});
        MeshRole scalar = catalog.add({"balsa.scalar", "Scalar"});
        MeshRole tangent = catalog.add({"balsa.tangent", "Tangent"});
        std::array<MeshRole, 2> texture_coordinates{
            catalog.add({"balsa.texture_coordinates_0", "Texture coordinates 0"}),
            catalog.add({"balsa.texture_coordinates_1", "Texture coordinates 1"})};
        MeshRole color = catalog.add({"balsa.color", "Color"});
        MeshRole visualized = catalog.add({"balsa.visualized", "Visualized attribute"});
    };

    auto defaults() -> Defaults & {
        static Defaults d;
        return d;
    }
} // namespace

auto mesh_roles() -> catalog::Catalog<MeshRoleDefinition> & { return defaults().catalog; }

namespace roles {
    auto position() -> MeshRole { return defaults().position; }
    auto normal() -> MeshRole { return defaults().normal; }
    auto scalar() -> MeshRole { return defaults().scalar; }
    auto tangent() -> MeshRole { return defaults().tangent; }
    auto texture_coordinates(std::size_t set) -> MeshRole {
        return defaults().texture_coordinates.at(set);
    }
    auto color() -> MeshRole { return defaults().color; }
    auto visualized() -> MeshRole { return defaults().visualized; }
} // namespace roles

} // namespace balsa::scene_graph
