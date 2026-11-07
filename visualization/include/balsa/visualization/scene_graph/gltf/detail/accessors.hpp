#pragma once
#include "../Accessor.hpp"
#include "../glTF.hpp"
#include "LoadErrors.hpp"

#include <cstddef>
#include <span>
#include <vector>

namespace balsa::visualization::scene_graph::gltf::detail {

// Byte layout of one accessor element.
struct AccessorLayout {
    std::size_t component_size; // bytes per component
    std::size_t rows;           // components per column (k for VECk/MATk)
    std::size_t columns;        // 1 except for MATk
    // Padded to 4 bytes for matrices.
    std::size_t column_stride;
    // Including column padding.
    std::size_t element_size;
};

auto component_size(ComponentType type) -> std::size_t;
auto accessor_layout(const Accessor &accessor) -> AccessorLayout;

// Validates accessors against the manifest.
void check_accessors(const glTF &manifest, LoadErrors &errors);

// Final values of accessors that can't be viewed in place (sparse, or
// without a bufferView); empty for the rest.
auto materialize_accessors(
    const glTF &manifest,
    const std::vector<std::span<const std::byte>> &buffer_views,
    LoadErrors &errors) -> std::vector<std::vector<std::byte>>;

} // namespace balsa::visualization::scene_graph::gltf::detail
