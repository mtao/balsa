#pragma once
#include "../glTF.hpp"
#include "LoadErrors.hpp"

namespace balsa::visualization::scene_graph::gltf::detail {

// Validates mesh primitives against the manifest.
void check_meshes(const glTF &manifest, LoadErrors &errors);

} // namespace balsa::visualization::scene_graph::gltf::detail
