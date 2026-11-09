#pragma once
#include "../glTF.hpp"
#include "LoadErrors.hpp"

#include <cstddef>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace balsa::visualization::gltf::detail {

inline constexpr std::string_view draco_extension = "KHR_draco_mesh_compression";

// Decoded values (tightly packed) of the accessors Draco-compressed
// primitives use, indexed like manifest.accessors; nullopt for others.
auto decode_draco_accessors(const glTF &manifest,
                            const std::vector<std::span<const std::byte>> &buffer_views,
                            LoadErrors &errors)
    -> std::vector<std::optional<std::vector<std::byte>>>;

} // namespace balsa::visualization::gltf::detail
