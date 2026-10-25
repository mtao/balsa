#include "balsa/visualization/scene_graph/gltf/Accessor.hpp"
#include <balsa/json/reflection.hpp>

namespace balsa::visualization::scene_graph::gltf {

auto to_json(nlohmann::json &j, const AccessorType &v) -> void {
    json::enum_to_json(j, v);
}
auto from_json(const nlohmann::json &j, AccessorType &v) -> void {
    json::enum_from_json(j, v);
}

auto to_json(nlohmann::json &j, const AccessorSparseIndices &v) -> void {
    json::to_json_members(j, v);
}
auto from_json(const nlohmann::json &j, AccessorSparseIndices &v) -> void {
    json::from_json_members(j, v);
}

auto to_json(nlohmann::json &j, const AccessorSparseValues &v) -> void {
    json::to_json_members(j, v);
}
auto from_json(const nlohmann::json &j, AccessorSparseValues &v) -> void {
    json::from_json_members(j, v);
}

auto to_json(nlohmann::json &j, const AccessorSparse &v) -> void {
    json::to_json_members(j, v);
}
auto from_json(const nlohmann::json &j, AccessorSparse &v) -> void {
    json::from_json_members(j, v);
}

auto to_json(nlohmann::json &j, const Accessor &v) -> void {
    json::to_json_members(j, v);
    // byteOffset MUST NOT be defined when bufferView is undefined.
    if (v.bufferView) {
        json::write_members<json::skip_default, ^^Accessor::byteOffset>(j, v);
    }
}
auto from_json(const nlohmann::json &j, Accessor &v) -> void {
    json::from_json_members(j, v);
}

} // namespace balsa::visualization::scene_graph::gltf
