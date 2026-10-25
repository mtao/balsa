#include "balsa/visualization/scene_graph/gltf/Animation.hpp"
#include <balsa/json/reflection.hpp>

namespace balsa::visualization::scene_graph::gltf {

auto to_json(nlohmann::json &j, const AnimationPath &v) -> void {
    json::enum_to_json(j, v);
}
auto from_json(const nlohmann::json &j, AnimationPath &v) -> void {
    json::enum_from_json(j, v);
}

auto to_json(nlohmann::json &j, const AnimationInterpolation &v) -> void {
    json::enum_to_json(j, v);
}
auto from_json(const nlohmann::json &j, AnimationInterpolation &v) -> void {
    json::enum_from_json(j, v);
}

auto to_json(nlohmann::json &j, const AnimationChannelTarget &v) -> void {
    json::to_json_members(j, v);
}
auto from_json(const nlohmann::json &j, AnimationChannelTarget &v) -> void {
    json::from_json_members(j, v);
}

auto to_json(nlohmann::json &j, const AnimationChannel &v) -> void {
    json::to_json_members(j, v);
}
auto from_json(const nlohmann::json &j, AnimationChannel &v) -> void {
    json::from_json_members(j, v);
}

auto to_json(nlohmann::json &j, const AnimationSampler &v) -> void {
    json::to_json_members(j, v);
}
auto from_json(const nlohmann::json &j, AnimationSampler &v) -> void {
    json::from_json_members(j, v);
}

auto to_json(nlohmann::json &j, const Animation &v) -> void {
    json::to_json_members(j, v);
}
auto from_json(const nlohmann::json &j, Animation &v) -> void {
    json::from_json_members(j, v);
}

} // namespace balsa::visualization::scene_graph::gltf
