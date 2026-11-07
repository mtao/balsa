#pragma once
#include <balsa/scene_graph/MeshAttribute.hpp>

#include <cstdint>
#include <optional>
#include <vulkan/vulkan.hpp>

namespace balsa::visualization::vulkan {

// The vertex format for `components` (1-4) values of `type` that a shader
// reads as float / vecN: SFLOAT for floats; UNORM / SNORM for normalized
// integers; USCALED / SSCALED for other 8 / 16-bit integers.  nullopt when
// there is none (32-bit integers, float64, more than 4 components).
auto vertex_format(scene_graph::AttributeType type,
                   uint8_t components,
                   bool normalized) -> std::optional<vk::Format>;

// Bytes per vertex of a format vertex_format returns; 0 for others.
auto vertex_format_size(vk::Format format) -> uint32_t;

} // namespace balsa::visualization::vulkan
