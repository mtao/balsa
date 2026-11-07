#pragma once
#include "Document.hpp"
#include "LoadError.hpp"
#include "Mesh.hpp"

#include <balsa/scene_graph/AttributeSemantic.hpp>
#include <balsa/scene_graph/MeshGeometry.hpp>

#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace balsa::visualization::scene_graph::gltf {

// MeshGeometry name for a glTF attribute semantic; POSITION and NORMAL
// map to balsa's "vertex_positions" / "vertex_normals".
auto attribute_name(std::string_view semantic) -> std::string;

// The balsa attribute semantic (see AttributeSemantic.hpp) and set for a
// glTF attribute semantic: POSITION, NORMAL, TANGENT, TEXCOORD_n and
// COLOR_n.  nullopt for others (JOINTS_n, WEIGHTS_n, _CUSTOM).
struct AttributeSemanticSet {
    balsa::scene_graph::AttributeSemantic semantic;
    std::uint8_t set = 0;
    auto operator==(const AttributeSemanticSet &) const -> bool = default;
};
auto attribute_semantic(std::string_view semantic)
    -> std::optional<AttributeSemanticSet>;

// A glTF mesh primitive as a MeshGeometry: its vertex attributes plus
// triangle or edge topology (strips, fans and loops expanded).
auto to_mesh_geometry(const Document &document,
                      std::size_t mesh,
                      std::size_t primitive)
    -> std::expected<balsa::scene_graph::MeshGeometry, LoadError>;

// A converted primitive with its mode and material.
struct MeshPrimitiveGeometry {
    balsa::scene_graph::MeshGeometry geometry;
    PrimitiveMode mode = PrimitiveMode::TRIANGLES;
    // Index into manifest().materials.
    std::optional<std::size_t> material;
};

// Every primitive of a glTF mesh.
auto to_mesh_geometries(const Document &document, std::size_t mesh)
    -> std::expected<std::vector<MeshPrimitiveGeometry>, LoadError>;

} // namespace balsa::visualization::scene_graph::gltf
