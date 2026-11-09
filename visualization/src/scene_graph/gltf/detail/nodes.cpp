#include "balsa/visualization/scene_graph/gltf/detail/nodes.hpp"

#include <algorithm>
#include <cstddef>
#include <format>
#include <ranges>
#include <vector>

namespace balsa::visualization::gltf::detail {

namespace {
    auto in_range(std::int64_t index, std::size_t size) -> bool {
        return index >= 0 && static_cast<std::size_t>(index) < size;
    }
} // namespace

void check_nodes(const glTF &manifest, LoadErrors &errors) {
    const auto node_count = manifest.nodes.size();
    const auto fail = [&](std::string message) {
        errors.add(LoadError::Code::InvalidGltf, std::move(message));
    };

    std::vector<std::size_t> parents(node_count, 0);
    for (const auto &[n, node] : std::views::enumerate(manifest.nodes)) {
        for (const auto child : node.children) {
            if (!in_range(child, node_count)) {
                fail(std::format("node {} refers to missing child node {}",
                                 n,
                                 child));
            } else if (++parents[static_cast<std::size_t>(child)] == 2) {
                fail(std::format("node {} has more than one parent", child));
            }
        }
        const auto check = [&](const std::optional<std::int64_t> &index,
                               std::size_t size,
                               std::string_view what) {
            if (index && !in_range(*index, size)) {
                fail(std::format("node {} refers to missing {} {}",
                                 n,
                                 what,
                                 *index));
            }
        };
        check(node.mesh, manifest.meshes.size(), "mesh");
        check(node.camera, manifest.cameras.size(), "camera");
        check(node.skin, manifest.skins.size(), "skin");
    }

    // With at most one parent each, unreachable nodes are on or below a
    // cycle.
    std::vector<bool> reached(node_count, false);
    std::vector<std::size_t> pending;
    for (std::size_t n = 0; n < node_count; ++n) {
        if (parents[n] == 0) { pending.push_back(n); }
    }
    while (!pending.empty()) {
        const auto n = pending.back();
        pending.pop_back();
        if (reached[n]) { continue; }
        reached[n] = true;
        for (const auto child : manifest.nodes[n].children) {
            if (in_range(child, node_count)) {
                pending.push_back(static_cast<std::size_t>(child));
            }
        }
    }
    if (const auto cyclic = std::ranges::find(reached, false);
        cyclic != reached.end()) {
        fail(std::format("node {} is part of a cycle",
                         std::ranges::distance(reached.begin(), cyclic)));
    }

    for (const auto &[s, scene] : std::views::enumerate(manifest.scenes)) {
        for (const auto node : scene.nodes) {
            if (!in_range(node, node_count)) {
                fail(std::format("scene {} refers to missing node {}", s, node));
            } else if (parents[static_cast<std::size_t>(node)] != 0) {
                fail(std::format("scene {} lists node {}, which is not a root "
                                 "node",
                                 s,
                                 node));
            }
        }
    }
    if (manifest.scene && !in_range(*manifest.scene, manifest.scenes.size())) {
        fail(std::format("default scene {} does not exist", *manifest.scene));
    }
}

} // namespace balsa::visualization::gltf::detail
