#include "balsa/scene_graph/MeshRole.hpp"

namespace balsa::scene_graph {

namespace {
    // Built-ins are added in declaration order.
    struct Defaults {
        catalog::Catalog<MeshRoleDefinition> catalog{"mesh role"};
        MeshRole position = catalog.add({"balsa.position", "Position"});
        MeshRole normal = catalog.add({"balsa.normal", "Normal"});
        MeshRole scalar = catalog.add({"balsa.scalar", "Scalar"});
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
} // namespace roles

} // namespace balsa::scene_graph
