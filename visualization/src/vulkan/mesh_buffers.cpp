#include "balsa/visualization/vulkan/mesh_buffers.hpp"
#include "balsa/visualization/vulkan/film.hpp"
#include "balsa/visualization/vulkan/vertex_format.hpp"

#include <spdlog/spdlog.h>
#include <zipper/MatrixBase.hxx>

#include <algorithm>
#include <limits>
#include <stdexcept>

namespace balsa::visualization::vulkan {

// ── Helper: create a device-local buffer and stage data into it ──────

namespace {
    VulkanBuffer make_device_buffer(Film &film,
                                    const void *data,
                                    vk::DeviceSize byte_size,
                                    vk::BufferUsageFlags usage) {
        VulkanBuffer buf(film,
                         byte_size,
                         usage | vk::BufferUsageFlagBits::eTransferDst,
                         vk::MemoryPropertyFlagBits::eDeviceLocal);
        buf.upload_staged(film, data, byte_size);
        return buf;
    }

    // `attribute` as floats (normalized integers scaled like Vulkan's
    // UNORM / SNORM formats).
    auto to_floats(const scene_graph::MeshAttribute &attribute)
        -> std::vector<float> {
        const bool normalized = attribute.normalized();
        return attribute.visit([&](const auto &v) {
            using T = std::remove_const_t<
                typename std::decay_t<decltype(v)>::element_type>;
            std::vector<float> floats(v.size());
            ::zipper::MatrixBase out(
                ::zipper::mdspan<float, ::zipper::dextents<2>,
                                 ::zipper::storage::col_major>(
                    floats.data(), v.extent(0), v.extent(1)));
            out = ::zipper::MatrixBase(v).template cast<float>();
            if constexpr (std::is_integral_v<T>) {
                if (normalized) {
                    constexpr float max = std::numeric_limits<T>::max();
                    std::ranges::transform(floats, floats.begin(), [](float x) {
                        return std::max(x / max, -1.0f);
                    });
                }
            }
            return floats;
        });
    }
} // namespace

// ── Vertex attributes ────────────────────────────────────────────────

auto MeshBuffers::attribute(Film &film,
                            const scene_graph::MeshAttributeHandle &attribute)
    -> const AttributeBuffer * {
    if (!attribute) return nullptr;
    if (const auto it = _attributes.find(&*attribute); it != _attributes.end()) {
        return &it->second.gpu;
    }
    const uint8_t components = attribute->component_count();
    const std::size_t count = attribute->count();
    if (!attribute->is_vector() || components < 1 || components > 4
        || count == 0) {
        spdlog::warn("MeshBuffers: attribute '{}' ({} components, {} values) "
                     "cannot be a vertex input",
                     attribute->name, components, count);
        return nullptr;
    }

    // Stored values are element-major, the packed layout vertex input
    // expects.  Upload them as they are if the device supports their
    // format, padded to 4 components if only that is supported, else as
    // float.
    const auto supported = [&](std::optional<vk::Format> format) {
        return format
               && bool(film.physical_device().getFormatProperties(*format).bufferFeatures
                       & vk::FormatFeatureFlagBits::eVertexBuffer);
    };
    const auto type = attribute->type();
    const bool normalized = attribute->normalized();
    const auto stored = attribute->visit([](const auto &v) {
        return std::as_bytes(std::span(v.data_handle(), v.size()));
    });

    std::vector<std::byte> staging;
    std::vector<float> floats;
    std::span<const std::byte> bytes;
    vk::Format format;
    if (const auto native = vertex_format(type, components, normalized);
        supported(native)) {
        format = *native;
        bytes = stored;
    } else if (const auto padded = components == 3
                                       ? vertex_format(type, 4, normalized)
                                       : std::nullopt;
               supported(padded)) {
        format = *padded;
        const auto size = scene_graph::attribute_type_size(type);
        staging.assign(count * 4 * size, std::byte{0});
        for (std::size_t i = 0; i < count; ++i) {
            std::ranges::copy(stored.subspan(i * 3 * size, 3 * size),
                              staging.begin()
                                  + static_cast<std::ptrdiff_t>(i * 4 * size));
        }
        bytes = staging;
    } else {
        format = *vertex_format(scene_graph::AttributeType::Float32,
                                components,
                                false);
        floats = to_floats(*attribute);
        bytes = std::as_bytes(std::span(floats));
    }

    auto &cached = _attributes[&*attribute];
    cached.source = attribute;
    cached.gpu = AttributeBuffer{
        .buffer = make_device_buffer(film, bytes.data(), bytes.size_bytes(),
                                     vk::BufferUsageFlagBits::eVertexBuffer),
        .format = format,
        .components = components,
        .count = static_cast<uint32_t>(count)};
    return &cached.gpu;
}

void MeshBuffers::clear_attributes() { _attributes.clear(); }

// ── Upload: index buffers ────────────────────────────────────────────

void MeshBuffers::upload_triangle_indices(Film &film,
                                          std::span<const uint32_t> data,
                                          uint32_t triangle_count) {
    if (data.size() < static_cast<size_t>(triangle_count) * 3) {
        throw std::runtime_error(
            "MeshBuffers::upload_triangle_indices: data too small for "
            "triangle_count");
    }
    _triangle_count = triangle_count;
    _triangle_indices = make_device_buffer(
        film,
        data.data(),
        static_cast<vk::DeviceSize>(triangle_count) * 3 * sizeof(uint32_t),
        vk::BufferUsageFlagBits::eIndexBuffer);
}

void MeshBuffers::upload_edge_indices(Film &film,
                                      std::span<const uint32_t> data,
                                      uint32_t edge_count) {
    if (data.size() < static_cast<size_t>(edge_count) * 2) {
        throw std::runtime_error(
            "MeshBuffers::upload_edge_indices: data too small for edge_count");
    }
    _edge_count = edge_count;
    _edge_indices = make_device_buffer(film,
                                       data.data(),
                                       static_cast<vk::DeviceSize>(edge_count)
                                           * 2 * sizeof(uint32_t),
                                       vk::BufferUsageFlagBits::eIndexBuffer);
}

// ── Release ──────────────────────────────────────────────────────────

void MeshBuffers::release() {
    _attributes.clear();
    _triangle_indices.release();
    _edge_indices.release();
    _triangle_count = 0;
    _edge_count = 0;
}

} // namespace balsa::visualization::vulkan
