#include "balsa/visualization/vulkan/mesh_buffers.hpp"
#include "balsa/visualization/vulkan/film.hpp"

#include <stdexcept>
#include <algorithm>

namespace balsa::visualization::vulkan {

// ── Helper: create a device-local buffer and stage data into it ──────

namespace {
    VulkanBuffer make_device_buffer(Film &film,
                                    const void *data,
                                    vk::DeviceSize byte_size,
                                    vk::BufferUsageFlags usage) {
        VulkanBuffer buf(
          film,
          byte_size,
          usage | vk::BufferUsageFlagBits::eTransferDst,
          vk::MemoryPropertyFlagBits::eDeviceLocal);
        buf.upload_staged(film, data, byte_size);
        return buf;
    }
}// namespace

// ── Upload: vertex attributes ────────────────────────────────────────

void MeshBuffers::upload_positions(Film &film, std::span<const float> data, uint32_t vertex_count) {
    if (data.size() < static_cast<size_t>(vertex_count) * 3) {
        throw std::runtime_error("MeshBuffers::upload_positions: data too small for vertex_count");
    }
    _vertex_count = vertex_count;
    _positions = make_device_buffer(
      film, data.data(), static_cast<vk::DeviceSize>(vertex_count) * 3 * sizeof(float), vk::BufferUsageFlagBits::eVertexBuffer);
}

void MeshBuffers::upload_normals(Film &film, std::span<const float> data) {
    if (data.size() < static_cast<size_t>(_vertex_count) * 3) {
        throw std::runtime_error("MeshBuffers::upload_normals: data too small for vertex_count (upload positions first)");
    }
    _normals = make_device_buffer(
      film, data.data(), static_cast<vk::DeviceSize>(_vertex_count) * 3 * sizeof(float), vk::BufferUsageFlagBits::eVertexBuffer);
}

void MeshBuffers::upload_colors(Film &film, std::span<const float> data) {
    if (data.size() < static_cast<size_t>(_vertex_count) * 3) {
        throw std::runtime_error("MeshBuffers::upload_colors: data too small for vertex_count (upload positions first)");
    }
    _colors = make_device_buffer(
      film, data.data(), static_cast<vk::DeviceSize>(_vertex_count) * 3 * sizeof(float), vk::BufferUsageFlagBits::eVertexBuffer);
}

void MeshBuffers::upload_scalars(Film &film, std::span<const float> data) {
    if (data.size() < static_cast<size_t>(_vertex_count)) {
        throw std::runtime_error("MeshBuffers::upload_scalars: data too small for vertex_count (upload positions first)");
    }
    _scalars = make_device_buffer(
      film, data.data(), static_cast<vk::DeviceSize>(_vertex_count) * sizeof(float), vk::BufferUsageFlagBits::eVertexBuffer);
}

// ── Upload: index buffers ────────────────────────────────────────────

void MeshBuffers::upload_triangle_indices(Film &film, std::span<const uint32_t> data, uint32_t triangle_count) {
    if (data.size() < static_cast<size_t>(triangle_count) * 3) {
        throw std::runtime_error("MeshBuffers::upload_triangle_indices: data too small for triangle_count");
    }
    _triangle_count = triangle_count;
    _triangle_indices = make_device_buffer(
      film, data.data(), static_cast<vk::DeviceSize>(triangle_count) * 3 * sizeof(uint32_t), vk::BufferUsageFlagBits::eIndexBuffer);
}

void MeshBuffers::upload_edge_indices(Film &film, std::span<const uint32_t> data, uint32_t edge_count) {
    if (data.size() < static_cast<size_t>(edge_count) * 2) {
        throw std::runtime_error("MeshBuffers::upload_edge_indices: data too small for edge_count");
    }
    _edge_count = edge_count;
    _edge_indices = make_device_buffer(
      film, data.data(), static_cast<vk::DeviceSize>(edge_count) * 2 * sizeof(uint32_t), vk::BufferUsageFlagBits::eIndexBuffer);
}

// ── Upload from size_t indices (narrowing conversion) ────────────────

void MeshBuffers::upload_triangle_indices_from_sizet(Film &film, std::span<const std::size_t> data, uint32_t triangle_count) {
    size_t count = static_cast<size_t>(triangle_count) * 3;
    if (data.size() < count) {
        throw std::runtime_error("MeshBuffers::upload_triangle_indices_from_sizet: data too small");
    }
    std::vector<uint32_t> converted(count);
    std::transform(data.begin(), data.begin() + count, converted.begin(), [](std::size_t v) -> uint32_t { return static_cast<uint32_t>(v); });
    upload_triangle_indices(film, converted, triangle_count);
}

void MeshBuffers::upload_edge_indices_from_sizet(Film &film, std::span<const std::size_t> data, uint32_t edge_count) {
    size_t count = static_cast<size_t>(edge_count) * 2;
    if (data.size() < count) {
        throw std::runtime_error("MeshBuffers::upload_edge_indices_from_sizet: data too small");
    }
    std::vector<uint32_t> converted(count);
    std::transform(data.begin(), data.begin() + count, converted.begin(), [](std::size_t v) -> uint32_t { return static_cast<uint32_t>(v); });
    upload_edge_indices(film, converted, edge_count);
}

// ── Release ──────────────────────────────────────────────────────────

void MeshBuffers::release() {
    _positions.release();
    _normals.release();
    _colors.release();
    _scalars.release();
    _triangle_indices.release();
    _edge_indices.release();
    _vertex_count = 0;
    _triangle_count = 0;
    _edge_count = 0;
}

// ── Vertex input descriptions ────────────────────────────────────────

std::vector<vk::VertexInputBindingDescription> MeshBuffers::binding_descriptions() const {
    std::vector<vk::VertexInputBindingDescription> bindings;

    // binding 0: positions (always present if we have vertices)
    if (has_positions()) {
        bindings.push_back({ 0, sizeof(float) * 3, vk::VertexInputRate::eVertex });
    }
    // binding 1: normals
    if (has_normals()) {
        bindings.push_back({ 1, sizeof(float) * 3, vk::VertexInputRate::eVertex });
    }
    // binding 2: colors
    if (has_colors()) {
        bindings.push_back({ 2, sizeof(float) * 3, vk::VertexInputRate::eVertex });
    }
    // binding 3: scalars
    if (has_scalars()) {
        bindings.push_back({ 3, sizeof(float) * 1, vk::VertexInputRate::eVertex });
    }

    return bindings;
}

std::vector<vk::VertexInputAttributeDescription> MeshBuffers::attribute_descriptions() const {
    std::vector<vk::VertexInputAttributeDescription> attrs;

    // location 0, binding 0: inPosition (vec3)
    if (has_positions()) {
        attrs.push_back({ 0, 0, vk::Format::eR32G32B32Sfloat, 0 });
    }
    // location 1, binding 1: inNormal (vec3)
    if (has_normals()) {
        attrs.push_back({ 1, 1, vk::Format::eR32G32B32Sfloat, 0 });
    }
    // location 2, binding 2: inColor (vec3)
    if (has_colors()) {
        attrs.push_back({ 2, 2, vk::Format::eR32G32B32Sfloat, 0 });
    }
    // location 3, binding 3: inScalar (float)
    if (has_scalars()) {
        attrs.push_back({ 3, 3, vk::Format::eR32Sfloat, 0 });
    }

    return attrs;
}

}// namespace balsa::visualization::vulkan
