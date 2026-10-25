#include "balsa/visualization/scene_graph/gltf/Material.hpp"
#include <balsa/json/reflection.hpp>

namespace balsa::visualization::scene_graph::gltf {

auto to_json(nlohmann::json &j, const AlphaMode &v) -> void {
    json::enum_to_json(j, v);
}
auto from_json(const nlohmann::json &j, AlphaMode &v) -> void {
    json::enum_from_json(j, v);
}

auto to_json(nlohmann::json &j, const TextureInfo &v) -> void {
    json::to_json_members(j, v);
}
auto from_json(const nlohmann::json &j, TextureInfo &v) -> void {
    json::from_json_members(j, v);
}

auto to_json(nlohmann::json &j, const MaterialNormalTextureInfo &v) -> void {
    json::to_json_members(j, v);
}
auto from_json(const nlohmann::json &j, MaterialNormalTextureInfo &v) -> void {
    json::from_json_members(j, v);
}

auto to_json(nlohmann::json &j, const MaterialOcclusionTextureInfo &v) -> void {
    json::to_json_members(j, v);
}
auto from_json(const nlohmann::json &j, MaterialOcclusionTextureInfo &v) -> void {
    json::from_json_members(j, v);
}

auto to_json(nlohmann::json &j, const MaterialPBRMetallicRoughness &v) -> void {
    json::to_json_members(j, v);
}
auto from_json(const nlohmann::json &j, MaterialPBRMetallicRoughness &v) -> void {
    json::from_json_members(j, v);
}

auto to_json(nlohmann::json &j, const Material &v) -> void {
    json::to_json_members(j, v);
    // alphaCutoff MUST NOT be defined unless alphaMode is; it is only
    // meaningful (and only written) in MASK mode.
    if (v.alphaMode == AlphaMode::MASK) {
        json::write_members<json::skip_default, ^^Material::alphaCutoff>(j, v);
    }
}
auto from_json(const nlohmann::json &j, Material &v) -> void {
    json::from_json_members(j, v);
}

} // namespace balsa::visualization::scene_graph::gltf
