#include "balsa/visualization/scene_graph/gltf/glTF.hpp"
#include <balsa/json/reflection.hpp>

namespace balsa::visualization::scene_graph::gltf {

auto to_json(nlohmann::json &j, const glTF &v) -> void {
    json::to_json_members(j, v);
}
auto from_json(const nlohmann::json &j, glTF &v) -> void {
    json::from_json_members(j, v);
}

} // namespace balsa::visualization::scene_graph::gltf
