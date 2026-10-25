#include "balsa/visualization/scene_graph/gltf/Mesh.hpp"
#include <balsa/json/reflection.hpp>

namespace balsa::visualization::scene_graph::gltf {

auto to_json(nlohmann::json &j, const MeshPrimitive &v) -> void {
    json::to_json_members(j, v);
}
auto from_json(const nlohmann::json &j, MeshPrimitive &v) -> void {
    json::from_json_members(j, v);
}

auto to_json(nlohmann::json &j, const Mesh &v) -> void {
    json::to_json_members(j, v);
}
auto from_json(const nlohmann::json &j, Mesh &v) -> void {
    json::from_json_members(j, v);
}

} // namespace balsa::visualization::scene_graph::gltf
