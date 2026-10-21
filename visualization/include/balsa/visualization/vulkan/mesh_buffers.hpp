#pragma once
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

// ── UBO structs (must match GLSL layout, std140 alignment) ───────────

// binding = 0 in mesh.vert / mesh.frag
struct TransformUBO {
    scene_graph::Mat4f model; // 64 bytes
    scene_graph::Mat4f view; // 64 bytes
    scene_graph::Mat4f projection; // 64 bytes
    scene_graph::Mat4f normal_matrix; // 64 bytes  (transpose(inverse(model)))
    scene_graph::Vec4f
        camera_pos; // 16 bytes  (xyz = world-space camera position, w = pad)
};
static_assert(sizeof(TransformUBO) == 272, "TransformUBO must be 272 bytes");

// binding = 1 in mesh.vert / mesh.frag
struct MaterialUBO {
    scene_graph::Vec4f uniform_color; // rgba
    scene_graph::Vec4f light_dir; // xyz = direction, w = ambient_strength
    scene_graph::Vec4f light_color; // xyz = light color, w = unused (pad)
    scene_graph::Vec4f specular_params; // xyz = specular_color, w = shininess
    scene_graph::Vec4f scalar_params; // x = scalar_min, y = scalar_max, z =
                                      // point_size, w = two_sided (>0.5)
    scene_graph::Vec4f
        layer_color; // rgba — per-layer color (solid/wireframe/point)
    scene_graph::Vec4f scalar_select; // x = mode (0 component, 1 p-norm),
                                      // y = component index or p,
                                      // z = component count, w = pad
};
static_assert(sizeof(MaterialUBO) == 112, "MaterialUBO must be 112 bytes");

// Maximum number of independently configured renderer layers drawn per mesh
// frame. The default preset uses three; applications may register more.
inline constexpr uint32_t k_max_material_layers = 16;

// Compute the aligned offset for dynamic UBO slots, respecting
// minUniformBufferOffsetAlignment.  Returns the stride in bytes
// between consecutive MaterialUBO slots.
inline vk::DeviceSize material_ubo_aligned_size(vk::DeviceSize min_alignment) {
    vk::DeviceSize raw = sizeof(MaterialUBO);
    if (min_alignment == 0) return raw;
    return (raw + min_alignment - 1) & ~(min_alignment - 1);
}

// ── AttributeBuffer / MeshVertexInputs ──────────────────────────────

// A vertex attribute uploaded as tightly packed floats, 1–4 components
// per vertex.  Vulkan fills components the shader reads but the format
// lacks with (0, 0, 0, 1).
struct AttributeBuffer {
    VulkanBuffer buffer;
    uint8_t components = 0;
    uint32_t count = 0;
};

// The vertex streams one draw reads, each the attribute a layer bound to
// the corresponding role (nullptr when unbound):
//
//   binding 0: positions (location 0)
//   binding 1: normals   (location 1)
//   binding 3: scalars   (location 3; reduced to one value in the shader)
struct MeshVertexInputs {
    const AttributeBuffer *positions = nullptr;
    const AttributeBuffer *normals = nullptr;
    const AttributeBuffer *scalars = nullptr;
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

    // ── Release all GPU resources ───────────────────────────────────

    void release();

    // ── Queries ─────────────────────────────────────────────────────

    uint32_t triangle_count() const { return _triangle_count; }
    uint32_t edge_count() const { return _edge_count; }
    bool has_triangle_indices() const { return _triangle_indices.is_valid(); }
    bool has_edge_indices() const { return _edge_indices.is_valid(); }

    vk::Buffer triangle_index_buffer() const {
        return _triangle_indices.buffer();
    }
    vk::Buffer edge_index_buffer() const { return _edge_indices.buffer(); }

  private:
    struct CachedAttribute {
        scene_graph::MeshAttributeHandle source; // keeps the key alive
        AttributeBuffer gpu;
    };
    std::unordered_map<const scene_graph::MeshAttribute *, CachedAttribute>
        _attributes;
    VulkanBuffer _triangle_indices;
    VulkanBuffer _edge_indices;

    uint32_t _triangle_count = 0;
    uint32_t _edge_count = 0;
};

} // namespace balsa::visualization::vulkan
