#include "balsa/visualization/scene_graph/gltf/Asset.hpp"
#include <balsa/json/reflection.hpp>

namespace balsa::visualization::scene_graph::gltf {

auto to_json(nlohmann::json &j, const Asset &v) -> void {
    json::to_json_members(j, v);
}
auto from_json(const nlohmann::json &j, Asset &v) -> void {
    json::from_json_members(j, v);
}

} // namespace balsa::visualization::scene_graph::gltf
