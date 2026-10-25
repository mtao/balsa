#include "balsa/visualization/scene_graph/gltf/Camera.hpp"
#include <balsa/json/reflection.hpp>

namespace balsa::visualization::scene_graph::gltf {

auto to_json(nlohmann::json &j, const CameraType &v) -> void {
    json::enum_to_json(j, v);
}
auto from_json(const nlohmann::json &j, CameraType &v) -> void {
    json::enum_from_json(j, v);
}

auto to_json(nlohmann::json &j, const CameraOrthographic &v) -> void {
    json::to_json_members(j, v);
}
auto from_json(const nlohmann::json &j, CameraOrthographic &v) -> void {
    json::from_json_members(j, v);
}

auto to_json(nlohmann::json &j, const CameraPerspective &v) -> void {
    json::to_json_members(j, v);
}
auto from_json(const nlohmann::json &j, CameraPerspective &v) -> void {
    json::from_json_members(j, v);
}

auto to_json(nlohmann::json &j, const Camera &v) -> void {
    json::to_json_members(j, v);
    // perspective and orthographic MUST NOT both be defined; write the one
    // selected by type.
    switch (v.type) {
    case CameraType::perspective:
        json::write_members<json::omit_empty, ^^Camera::perspective>(j, v);
        break;
    case CameraType::orthographic:
        json::write_members<json::omit_empty, ^^Camera::orthographic>(j, v);
        break;
    }
}
auto from_json(const nlohmann::json &j, Camera &v) -> void {
    json::from_json_members(j, v);
}

} // namespace balsa::visualization::scene_graph::gltf
