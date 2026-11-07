#pragma once
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <balsa/catalog/Catalog.hpp>

namespace balsa::scene_graph {

// An input a mesh renderer reads (position, normal, scalar, ...).  Declared
// via MeshRoleRequirement, bound per layer.  Extensible via mesh_roles().
struct MeshRoleDefinition {
    std::string id;
    std::string label;
};

using MeshRole = catalog::Entry<MeshRoleDefinition>;

// Default catalog, with the built-ins below.
auto mesh_roles() -> catalog::Catalog<MeshRoleDefinition> &;

// Built-in roles.
namespace roles {
    auto position() -> MeshRole; // "balsa.position"
    auto normal() -> MeshRole;   // "balsa.normal"
    auto scalar() -> MeshRole;   // "balsa.scalar"
} // namespace roles

} // namespace balsa::scene_graph
