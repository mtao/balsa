#include "balsa/visualization/vulkan/mesh_buffers.hpp"
#include "balsa/visualization/vulkan/film.hpp"

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

// ── Upload: vertex attributes ────────────────────────────────────────

void MeshBuffers::upload_positions(Film &film,
                                   const void *data,
                                   std::size_t byte_size,
                                   uint32_t vertex_count,
                                   uint8_t components) {
    if (components < 1 || components > 4) {
        throw std::runtime_error(
            "MeshBuffers::upload_positions: components must be 1–4");
    }
    std::size_t expected =
        static_cast<std::size_t>(vertex_count) * components * sizeof(float);
    if (byte_size < expected) {
        throw std::runtime_error(
            "MeshBuffers::upload_positions: data too small for vertex_count × "
            "components");
    }
    _vertex_count = vertex_count;
    _position_components = components;
    _positions = make_device_buffer(film,
                                    data,
                                    static_cast<vk::DeviceSize>(expected),
                                    vk::BufferUsageFlagBits::eVertexBuffer);
}

void MeshBuffers::upload_normals(Film &film,
                                 const void *data,
                                 std::size_t byte_size,
                                 uint8_t components) {
    if (components < 1 || components > 4) {
        throw std::runtime_error(
            "MeshBuffers::upload_normals: components must be 1–4");
    }
    std::size_t expected =
        static_cast<std::size_t>(_vertex_count) * components * sizeof(float);
    if (byte_size < expected) {
        throw std::runtime_error(
            "MeshBuffers::upload_normals: data too small for vertex_count × "
            "components (upload positions first)");
    }
    _normal_components = components;
    _normals = make_device_buffer(film,
                                  data,
                                  static_cast<vk::DeviceSize>(expected),
                                  vk::BufferUsageFlagBits::eVertexBuffer);
}

void MeshBuffers::upload_colors(Film &film, std::span<const float> data) {
    if (data.size() < static_cast<size_t>(_vertex_count) * 4) {
        throw std::runtime_error(
            "MeshBuffers::upload_colors: data too small for vertex_count "
            "(upload positions first)");
    }
    _colors = make_device_buffer(film,
                                 data.data(),
                                 static_cast<vk::DeviceSize>(_vertex_count) * 4
                                     * sizeof(float),
                                 vk::BufferUsageFlagBits::eVertexBuffer);
}

void MeshBuffers::upload_scalars(Film &film, std::span<const float> data) {
    if (data.size() < static_cast<size_t>(_vertex_count)) {
        throw std::runtime_error(
            "MeshBuffers::upload_scalars: data too small for vertex_count "
            "(upload positions first)");
    }
    _scalars = make_device_buffer(film,
                                  data.data(),
                                  static_cast<vk::DeviceSize>(_vertex_count)
                                      * sizeof(float),
                                  vk::BufferUsageFlagBits::eVertexBuffer);
}

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

// ── Upload from size_t indices (narrowing conversion) ────────────────

void MeshBuffers::upload_triangle_indices_from_sizet(
    Film &film,
    std::span<const std::size_t> data,
    uint32_t triangle_count) {
    size_t count = static_cast<size_t>(triangle_count) * 3;
    if (data.size() < count) {
        throw std::runtime_error(
            "MeshBuffers::upload_triangle_indices_from_sizet: data too small");
    }
    std::vector<uint32_t> converted(count);
    std::transform(
        data.begin(),
        data.begin() + count,
        converted.begin(),
        [](std::size_t v) -> uint32_t { return static_cast<uint32_t>(v); });
    upload_triangle_indices(film, converted, triangle_count);
}

void MeshBuffers::upload_edge_indices_from_sizet(
    Film &film,
    std::span<const std::size_t> data,
    uint32_t edge_count) {
    size_t count = static_cast<size_t>(edge_count) * 2;
    if (data.size() < count) {
        throw std::runtime_error(
            "MeshBuffers::upload_edge_indices_from_sizet: data too small");
    }
    std::vector<uint32_t> converted(count);
    std::transform(
        data.begin(),
        data.begin() + count,
        converted.begin(),
        [](std::size_t v) -> uint32_t { return static_cast<uint32_t>(v); });
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
    _position_components = 3;
    _normal_components = 3;
}

// ── Vertex input descriptions ────────────────────────────────────────

namespace {
    // Map component count (1–4) to the corresponding VkFormat for float
    // attributes.
    vk::Format float_format_for_components(uint8_t components) {
        switch (components) {
        case 1:
            return vk::Format::eR32Sfloat;
        case 2:
            return vk::Format::eR32G32Sfloat;
        case 3:
            return vk::Format::eR32G32B32Sfloat;
        case 4:
            return vk::Format::eR32G32B32A32Sfloat;
        default:
            return vk::Format::eR32G32B32Sfloat; // fallback
        }
    }
} // namespace

std::vector<vk::VertexInputBindingDescription>
    MeshBuffers::binding_descriptions() const {
    std::vector<vk::VertexInputBindingDescription> bindings;

    // binding 0: positions (variable component count)
    if (has_positions()) {
        bindings.push_back(
            {0,
             static_cast<uint32_t>(sizeof(float) * _position_components),
             vk::VertexInputRate::eVertex});
    }
    // binding 1: normals (variable component count)
    if (has_normals()) {
        bindings.push_back(
            {1,
             static_cast<uint32_t>(sizeof(float) * _normal_components),
             vk::VertexInputRate::eVertex});
    }
    // binding 2: colors
    if (has_colors()) {
        bindings.push_back(
            {2, sizeof(float) * 4, vk::VertexInputRate::eVertex});
    }
    // binding 3: scalars
    if (has_scalars()) {
        bindings.push_back(
            {3, sizeof(float) * 1, vk::VertexInputRate::eVertex});
    }

    return bindings;
}

std::vector<vk::VertexInputAttributeDescription>
    MeshBuffers::attribute_descriptions() const {
    std::vector<vk::VertexInputAttributeDescription> attrs;

    // location 0, binding 0: inPosition (vec2/vec3/vec4, auto-fills missing)
    if (has_positions()) {
        attrs.push_back(
            {0, 0, float_format_for_components(_position_components), 0});
    }
    // location 1, binding 1: inNormal (vec2/vec3/vec4, auto-fills missing)
    if (has_normals()) {
        attrs.push_back(
            {1, 1, float_format_for_components(_normal_components), 0});
    }
    // location 2, binding 2: inColor (vec4)
    if (has_colors()) {
        attrs.push_back({2, 2, vk::Format::eR32G32B32A32Sfloat, 0});
    }
    // location 3, binding 3: inScalar (float)
    if (has_scalars()) { attrs.push_back({3, 3, vk::Format::eR32Sfloat, 0}); }

    return attrs;
}

} // namespace balsa::visualization::vulkan
