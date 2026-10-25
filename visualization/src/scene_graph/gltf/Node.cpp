#include "balsa/visualization/scene_graph/gltf/Node.hpp"
#include <balsa/json/reflection.hpp>

namespace balsa::visualization::scene_graph::gltf {

auto to_json(nlohmann::json &j, const Node &v) -> void {
    json::to_json_members(j, v);
    // matrix MUST NOT be defined alongside translation/rotation/scale. A
    // non-identity matrix takes precedence; otherwise write the TRS.
    static const Node default_node{};
    if (v.matrix != default_node.matrix) {
        json::write_members<^^Node::matrix>(j, v);
    } else {
        json::write_members<json::skip_default, ^^Node::translation,
                                   ^^Node::rotation, ^^Node::scale>(j, v);
    }
}
auto from_json(const nlohmann::json &j, Node &v) -> void {
    json::from_json_members(j, v);
}

} // namespace balsa::visualization::scene_graph::gltf
