#pragma once
#include "../glTF.hpp"
#include "LoadErrors.hpp"

namespace balsa::visualization::gltf::detail {

// Validates mesh primitives against the manifest.
void check_meshes(const glTF &manifest, LoadErrors &errors);

} // namespace balsa::visualization::gltf::detail
