#if !defined(BALSA_VISUALIZATION_VULKAN_MESH_BUFFERS_HPP)
#define BALSA_VISUALIZATION_VULKAN_MESH_BUFFERS_HPP

#include <cstdint>
#include <optional>
#include <span>
#include <vector>
#include <vulkan/vulkan.hpp>

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
};
static_assert(sizeof(MaterialUBO) == 96, "MaterialUBO must be 96 bytes");

// Maximum number of render layers drawn per mesh per frame (solid,
// wireframe, points).  Used to size the dynamic MaterialUBO buffer.
inline constexpr uint32_t k_max_material_layers = 3;

// Compute the aligned offset for dynamic UBO slots, respecting
// minUniformBufferOffsetAlignment.  Returns the stride in bytes
// between consecutive MaterialUBO slots.
inline vk::DeviceSize material_ubo_aligned_size(vk::DeviceSize min_alignment) {
    vk::DeviceSize raw = sizeof(MaterialUBO);
    if (min_alignment == 0) return raw;
    return (raw + min_alignment - 1) & ~(min_alignment - 1);
}

// ── MeshBuffers ──────────────────────────────────────────────────────
//
// Owns GPU-side VulkanBuffers for mesh vertex attributes and index
// buffers.  Each attribute is an optional separate VkBuffer (matching
// the separate-binding vertex layout in mesh.vert).
//
//   binding 0: positions   (vec2/vec3, location 0)  — format-aware
//   binding 1: normals     (vec2/vec3, location 1)  — format-aware
//   binding 2: colors      (vec4,      location 2)
//   binding 3: scalars     (float,     location 3)
//
// Positions and normals support variable component counts (1–4 float
// components).  Vulkan auto-fills missing components in the vertex
// shader: a R32G32_SFLOAT attribute read as vec3 yields (x, y, 0.0).
//
// Two index buffers:
//   triangle_indices  (uint32, for solid/flat/phong draws)
//   edge_indices      (uint32, for wireframe draws)

class MeshBuffers {
  public:
    MeshBuffers() = default;
    ~MeshBuffers() = default;

    // Move-only (VulkanBuffer is move-only)
    MeshBuffers(const MeshBuffers &) = delete;
    MeshBuffers &operator=(const MeshBuffers &) = delete;
    MeshBuffers(MeshBuffers &&) = default;
    MeshBuffers &operator=(MeshBuffers &&) = default;

    // ── Upload raw data ─────────────────────────────────────────────

    // Positions: raw float data with variable component count (1–4).
    // byte_size = vertex_count * components * sizeof(float).
    // Vulkan auto-fills missing components when the shader reads vec3.
    void upload_positions(Film &film,
                          const void *data,
                          std::size_t byte_size,
                          uint32_t vertex_count,
                          uint8_t components);

    // Normals: raw float data with variable component count (1–4).
    // byte_size = vertex_count * components * sizeof(float).
    void upload_normals(Film &film,
                        const void *data,
                        std::size_t byte_size,
                        uint8_t components);

    // Per-vertex colors: N vertices × 4 floats (tightly packed vec4[], RGBA).
    void upload_colors(Film &film, std::span<const float> data);

    // Per-vertex scalars: N vertices × 1 float.
    void upload_scalars(Film &film, std::span<const float> data);

    // Triangle indices: T triangles × 3 uint32.
    void upload_triangle_indices(Film &film,
                                 std::span<const uint32_t> data,
                                 uint32_t triangle_count);

    // Edge indices: E edges × 2 uint32.
    void upload_edge_indices(Film &film,
                             std::span<const uint32_t> data,
                             uint32_t edge_count);

    // ── Upload from size_t index data (performs narrowing conversion) ─

    void upload_triangle_indices_from_sizet(Film &film,
                                            std::span<const std::size_t> data,
                                            uint32_t triangle_count);
    void upload_edge_indices_from_sizet(Film &film,
                                        std::span<const std::size_t> data,
                                        uint32_t edge_count);

    // ── Release all GPU resources ───────────────────────────────────

    void release();

    // ── Queries ─────────────────────────────────────────────────────

    uint32_t vertex_count() const { return _vertex_count; }
    uint32_t triangle_count() const { return _triangle_count; }
    uint32_t edge_count() const { return _edge_count; }

    // Number of float components per position vertex (1–4, default 3).
    uint8_t position_components() const { return _position_components; }

    // Number of float components per normal vertex (1–4, default 3).
    uint8_t normal_components() const { return _normal_components; }

    bool has_positions() const { return _positions.is_valid(); }
    bool has_normals() const { return _normals.is_valid(); }
    bool has_colors() const { return _colors.is_valid(); }
    bool has_scalars() const { return _scalars.is_valid(); }
    bool has_triangle_indices() const { return _triangle_indices.is_valid(); }
    bool has_edge_indices() const { return _edge_indices.is_valid(); }

    // Raw buffer access (for binding in draw commands)
    vk::Buffer positions_buffer() const { return _positions.buffer(); }
    vk::Buffer normals_buffer() const { return _normals.buffer(); }
    vk::Buffer colors_buffer() const { return _colors.buffer(); }
    vk::Buffer scalars_buffer() const { return _scalars.buffer(); }
    vk::Buffer triangle_index_buffer() const {
        return _triangle_indices.buffer();
    }
    vk::Buffer edge_index_buffer() const { return _edge_indices.buffer(); }

    // ── Vertex input descriptions ───────────────────────────────────
    //
    // Return Vulkan vertex input binding/attribute descriptions based
    // on which attributes are currently populated.  Uses stored
    // component counts for position/normal formats.

    std::vector<vk::VertexInputBindingDescription> binding_descriptions() const;
    std::vector<vk::VertexInputAttributeDescription>
        attribute_descriptions() const;

  private:
    VulkanBuffer _positions;
    VulkanBuffer _normals;
    VulkanBuffer _colors;
    VulkanBuffer _scalars;
    VulkanBuffer _triangle_indices;
    VulkanBuffer _edge_indices;

    uint32_t _vertex_count = 0;
    uint32_t _triangle_count = 0;
    uint32_t _edge_count = 0;
    uint8_t _position_components = 3;
    uint8_t _normal_components = 3;
};

} // namespace balsa::visualization::vulkan

#endif
