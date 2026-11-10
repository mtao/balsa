#pragma once
#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <unordered_map>
#include <vector>
#include <vulkan/vulkan.hpp>

#include "balsa/scene_graph/MeshAttribute.hpp"
#include "balsa/scene_graph/types.hpp"
#include "balsa/visualization/vulkan/buffer.hpp"

namespace balsa::visualization::vulkan {

class Film;

// ── Set 0 uniform blocks (std140; "balsa/scene.glsl") ─────────────────

// Binding 0.
struct TransformUBO {
    scene_graph::Mat4f model;
    scene_graph::Mat4f view;
    scene_graph::Mat4f projection;
    scene_graph::Mat4f normal_matrix; // transpose(inverse(model))
    scene_graph::Vec4f camera_pos;    // xyz = world-space camera position
};
static_assert(sizeof(TransformUBO) == 272, "TransformUBO must be 272 bytes");

// Binding 1.
struct SceneLightingUBO {
    scene_graph::Vec4f light_direction; // xyz = world-space direction to the light
    scene_graph::Vec4f light_color;     // xyz = colour, w = 1 if the light is on
};
static_assert(sizeof(SceneLightingUBO) == 32, "SceneLightingUBO must be 32 bytes");

// Maximum number of independently configured renderer layers drawn per mesh
// frame. The default preset uses three; applications may register more.
inline constexpr uint32_t k_max_material_layers = 16;

// ── AttributeBuffer ──────────────────────────────────────────────────

// A vertex attribute on the GPU, 1–4 components per vertex, in a format the
// shader reads as float (see vertex_format).  Vulkan fills components the
// shader reads but the format lacks with (0, 0, 0, 1).
struct AttributeBuffer {
    VulkanBuffer buffer;
    vk::Format format = vk::Format::eUndefined;
    uint8_t components = 0; // of the attribute (format may be padded to 4)
    uint32_t count = 0;
};

// ── MeshBuffers ──────────────────────────────────────────────────────
//
// GPU buffers for one mesh: a cache of uploaded vertex attributes, keyed
// by attribute identity so layers binding the same attribute share one
// buffer, plus the triangle and edge index buffers.

class MeshBuffers {
  public:
    MeshBuffers() = default;
    ~MeshBuffers() = default;

    // Move-only (VulkanBuffer is move-only)
    MeshBuffers(const MeshBuffers &) = delete;
    MeshBuffers &operator=(const MeshBuffers &) = delete;
    MeshBuffers(MeshBuffers &&) = default;
    MeshBuffers &operator=(MeshBuffers &&) = default;

    // ── Vertex attributes ───────────────────────────────────────────

    // The uploaded buffer for attribute, uploading it (converted to
    // float) on first use.  nullptr for an invalid handle, an empty
    // attribute, or more than 4 components.
    auto attribute(Film &film, const scene_graph::MeshAttributeHandle &attribute)
        -> const AttributeBuffer *;

    // Drop all uploaded attributes, e.g. after their values changed.
    void clear_attributes();

    // ── Index buffers ───────────────────────────────────────────────

    // Triangle indices: T triangles × 3 uint32.
    void upload_triangle_indices(Film &film,
                                 std::span<const uint32_t> data,
                                 uint32_t triangle_count);

    // Edge indices: E edges × 2 uint32.
    void upload_edge_indices(Film &film,
                             std::span<const uint32_t> data,
                             uint32_t edge_count);

    // Point indices: the vertex subset to draw as points.
    void upload_point_indices(Film &film, std::span<const uint32_t> data);

    // ── Release all GPU resources ───────────────────────────────────

    void release();

    // ── Queries ─────────────────────────────────────────────────────

    uint32_t triangle_count() const { return _triangle_count; }
    uint32_t edge_count() const { return _edge_count; }
    uint32_t point_count() const { return _point_count; }
    bool has_triangle_indices() const { return _triangle_indices.is_valid(); }
    bool has_edge_indices() const { return _edge_indices.is_valid(); }
    bool has_point_indices() const { return _point_indices.is_valid(); }

    vk::Buffer triangle_index_buffer() const {
        return _triangle_indices.buffer();
    }
    vk::Buffer edge_index_buffer() const { return _edge_indices.buffer(); }
    vk::Buffer point_index_buffer() const { return _point_indices.buffer(); }

  private:
    struct CachedAttribute {
        scene_graph::MeshAttributeHandle source; // keeps the key alive
        AttributeBuffer gpu;
    };
    std::unordered_map<const scene_graph::MeshAttribute *, CachedAttribute>
        _attributes;
    VulkanBuffer _triangle_indices;
    VulkanBuffer _edge_indices;
    VulkanBuffer _point_indices;

    uint32_t _triangle_count = 0;
    uint32_t _edge_count = 0;
    uint32_t _point_count = 0;
};

} // namespace balsa::visualization::vulkan
