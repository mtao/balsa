#include "balsa/visualization/vulkan/vertex_format.hpp"

#include <algorithm>
#include <array>
#include <utility>

namespace balsa::visualization::vulkan {

namespace {
    using scene_graph::AttributeType;

    // AttributeType values follow VkComponentTypeKHR.
    template <AttributeType A, vk::ComponentTypeKHR V>
    constexpr bool same_value =
        std::to_underlying(A) == static_cast<uint32_t>(V);
    static_assert(same_value<AttributeType::Float16, vk::ComponentTypeKHR::eFloat16>);
    static_assert(same_value<AttributeType::Float32, vk::ComponentTypeKHR::eFloat32>);
    static_assert(same_value<AttributeType::Float64, vk::ComponentTypeKHR::eFloat64>);
    static_assert(same_value<AttributeType::Int8, vk::ComponentTypeKHR::eSint8>);
    static_assert(same_value<AttributeType::Int16, vk::ComponentTypeKHR::eSint16>);
    static_assert(same_value<AttributeType::Int32, vk::ComponentTypeKHR::eSint32>);
    static_assert(same_value<AttributeType::Int64, vk::ComponentTypeKHR::eSint64>);
    static_assert(same_value<AttributeType::UInt8, vk::ComponentTypeKHR::eUint8>);
    static_assert(same_value<AttributeType::UInt16, vk::ComponentTypeKHR::eUint16>);
    static_assert(same_value<AttributeType::UInt32, vk::ComponentTypeKHR::eUint32>);
    static_assert(same_value<AttributeType::UInt64, vk::ComponentTypeKHR::eUint64>);

    struct Entry {
        AttributeType type;
        bool normalized;
        std::array<vk::Format, 4> formats; // for 1-4 components
    };
    using F = vk::Format;
    constexpr std::array entries{
        Entry{AttributeType::Float16, false,
              {F::eR16Sfloat, F::eR16G16Sfloat, F::eR16G16B16Sfloat,
               F::eR16G16B16A16Sfloat}},
        Entry{AttributeType::Float32, false,
              {F::eR32Sfloat, F::eR32G32Sfloat, F::eR32G32B32Sfloat,
               F::eR32G32B32A32Sfloat}},
        Entry{AttributeType::Int8, true,
              {F::eR8Snorm, F::eR8G8Snorm, F::eR8G8B8Snorm, F::eR8G8B8A8Snorm}},
        Entry{AttributeType::Int8, false,
              {F::eR8Sscaled, F::eR8G8Sscaled, F::eR8G8B8Sscaled,
               F::eR8G8B8A8Sscaled}},
        Entry{AttributeType::UInt8, true,
              {F::eR8Unorm, F::eR8G8Unorm, F::eR8G8B8Unorm, F::eR8G8B8A8Unorm}},
        Entry{AttributeType::UInt8, false,
              {F::eR8Uscaled, F::eR8G8Uscaled, F::eR8G8B8Uscaled,
               F::eR8G8B8A8Uscaled}},
        Entry{AttributeType::Int16, true,
              {F::eR16Snorm, F::eR16G16Snorm, F::eR16G16B16Snorm,
               F::eR16G16B16A16Snorm}},
        Entry{AttributeType::Int16, false,
              {F::eR16Sscaled, F::eR16G16Sscaled, F::eR16G16B16Sscaled,
               F::eR16G16B16A16Sscaled}},
        Entry{AttributeType::UInt16, true,
              {F::eR16Unorm, F::eR16G16Unorm, F::eR16G16B16Unorm,
               F::eR16G16B16A16Unorm}},
        Entry{AttributeType::UInt16, false,
              {F::eR16Uscaled, F::eR16G16Uscaled, F::eR16G16B16Uscaled,
               F::eR16G16B16A16Uscaled}},
    };
} // namespace

auto vertex_format(AttributeType type, uint8_t components, bool normalized)
    -> std::optional<vk::Format> {
    if (components < 1 || components > 4) { return std::nullopt; }
    const auto entry = std::ranges::find_if(entries, [&](const Entry &e) {
        return e.type == type && e.normalized == normalized;
    });
    if (entry == entries.end()) { return std::nullopt; }
    return entry->formats[components - 1];
}

auto vertex_format_size(vk::Format format) -> uint32_t {
    for (const auto &entry : entries) {
        if (const auto it = std::ranges::find(entry.formats, format);
            it != entry.formats.end()) {
            const auto components =
                static_cast<uint32_t>(it - entry.formats.begin()) + 1;
            return components
                   * static_cast<uint32_t>(
                       scene_graph::attribute_type_size(entry.type));
        }
    }
    return 0;
}

} // namespace balsa::visualization::vulkan
