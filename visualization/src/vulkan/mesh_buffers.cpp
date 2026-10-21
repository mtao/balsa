#include "balsa/visualization/vulkan/mesh_buffers.hpp"
#include "balsa/visualization/vulkan/film.hpp"

#include <spdlog/spdlog.h>
#include <zipper/MatrixBase.hxx>

#include <algorithm>
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
    if (components < 1 || components > 4 || count == 0) {
        spdlog::warn("MeshBuffers: attribute '{}' ({} components, {} values) "
                     "cannot be a vertex input",
                     attribute->name, components, count);
        return nullptr;
    }

    // Values are column-major (components x count), i.e. element-major,
    // which is the packed layout the vertex input expects.
    std::vector<float> staging;
    const std::span<const float> values = attribute->visit(
        [&](const auto &v) -> std::span<const float> {
            using T = std::remove_const_t<
                typename std::decay_t<decltype(v)>::element_type>;
            if constexpr (std::is_same_v<T, float>) {
                return {v.data_handle(), v.size()};
            } else {
                staging.resize(v.size());
                ::zipper::MatrixBase out(
                    ::zipper::mdspan<float, ::zipper::dextents<2>,
                                     ::zipper::storage::col_major>(
                        staging.data(), v.extent(0), v.extent(1)));
                out = ::zipper::MatrixBase(v).template cast<float>();
                return staging;
            }
        });

    auto &cached = _attributes[&*attribute];
    cached.source = attribute;
    cached.gpu = AttributeBuffer{
        .buffer = make_device_buffer(film, values.data(), values.size_bytes(),
                                     vk::BufferUsageFlagBits::eVertexBuffer),
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
