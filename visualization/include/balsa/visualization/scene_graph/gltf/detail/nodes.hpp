#pragma once
#include "../glTF.hpp"
#include "LoadErrors.hpp"

namespace balsa::visualization::scene_graph::gltf::detail {

// Validates the node hierarchy and scenes against the manifest.
void check_nodes(const glTF &manifest, LoadErrors &errors);

} // namespace balsa::visualization::scene_graph::gltf::detail
