#include "balsa/visualization/vulkan/mesh_drawable.hpp"
#include "balsa/visualization/vulkan/film.hpp"
#include "balsa/visualization/vulkan/mesh_pipeline.hpp"
#include <glm/gtc/matrix_inverse.hpp>
#include <spdlog/spdlog.h>

namespace balsa::visualization::vulkan {

// ── Destructor ───────────────────────────────────────────────────────

MeshDrawable::~MeshDrawable() {
    release();
}

// ── Move ─────────────────────────────────────────────────────────────

MeshDrawable::MeshDrawable(MeshDrawable &&o) noexcept
  : render_state(std::move(o.render_state)),
    model_matrix(o.model_matrix),
    name(std::move(o.name)),
    visible(o.visible),
    _buffers(std::move(o._buffers)),
    _transform_ubo(std::move(o._transform_ubo)),
    _material_ubo(std::move(o._material_ubo)),
    _descriptor_set(o._descriptor_set),
    _initialized(o._initialized) {
    o._descriptor_set = vk::DescriptorSet{};
    o._initialized = false;
}

MeshDrawable &MeshDrawable::operator=(MeshDrawable &&o) noexcept {
    if (this != &o) {
        release();
        render_state = std::move(o.render_state);
        model_matrix = o.model_matrix;
        name = std::move(o.name);
        visible = o.visible;
        _buffers = std::move(o._buffers);
        _transform_ubo = std::move(o._transform_ubo);
        _material_ubo = std::move(o._material_ubo);
        _descriptor_set = o._descriptor_set;
        _initialized = o._initialized;
        o._descriptor_set = vk::DescriptorSet{};
        o._initialized = false;
    }
    return *this;
}

// ── init ─────────────────────────────────────────────────────────────

void MeshDrawable::init(Film &film, MeshPipelineManager &manager) {
    if (_initialized) return;

    // Create host-visible UBO buffers (small, updated every frame)
    _transform_ubo = VulkanBuffer(
      film,
      sizeof(TransformUBO),
      vk::BufferUsageFlagBits::eUniformBuffer,
      vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

    _material_ubo = VulkanBuffer(
      film,
      sizeof(MaterialUBO),
      vk::BufferUsageFlagBits::eUniformBuffer,
      vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

    // Allocate descriptor set and write UBO bindings
    _descriptor_set = manager.allocate_descriptor_set();
    manager.write_descriptor_set(
      _descriptor_set,
      _transform_ubo.buffer(),
      sizeof(TransformUBO),
      _material_ubo.buffer(),
      sizeof(MaterialUBO));

    _initialized = true;
}

// ── update_ubos ──────────────────────────────────────────────────────

void MeshDrawable::update_ubos(const glm::mat4 &view, const glm::mat4 &projection) {
    if (!_initialized) return;

    // TransformUBO
    TransformUBO transform;
    transform.model = model_matrix;
    transform.view = view;
    transform.projection = projection;
    transform.normal_matrix = glm::inverseTranspose(model_matrix);
    _transform_ubo.upload(&transform, sizeof(TransformUBO));

    // MaterialUBO — pack from render_state
    MaterialUBO material;
    material.uniform_color = glm::vec4(
      render_state.uniform_color[0],
      render_state.uniform_color[1],
      render_state.uniform_color[2],
      render_state.uniform_color[3]);
    material.light_dir = glm::vec4(
      render_state.light_dir[0],
      render_state.light_dir[1],
      render_state.light_dir[2],
      render_state.ambient_strength);
    material.specular_params = glm::vec4(
      render_state.specular_strength,
      render_state.specular_strength,
      render_state.specular_strength,
      render_state.shininess);
    material.scalar_params = glm::vec4(
      render_state.scalar_min,
      render_state.scalar_max,
      render_state.point_size,
      render_state.two_sided ? 1.0f : 0.0f);
    material.wireframe_color = glm::vec4(
      render_state.wireframe_color[0],
      render_state.wireframe_color[1],
      render_state.wireframe_color[2],
      render_state.wireframe_color[3]);
    _material_ubo.upload(&material, sizeof(MaterialUBO));
}

// ── draw ─────────────────────────────────────────────────────────────

void MeshDrawable::draw(Film &film, MeshPipelineManager &manager) {
    if (!_initialized || !visible) return;
    if (!_buffers.has_positions()) return;

    // Get (or create) the pipeline for the current state
    auto pipeline = manager.get_or_create(render_state, _buffers, film);
    if (!pipeline) return;

    auto cb = film.current_command_buffer();

    // Bind pipeline
    cb.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline);

    // Set dynamic line width for wireframe mode
    if (render_state.render_mode == RenderMode::Wireframe || render_state.render_mode == RenderMode::SolidWireframe) {
        cb.setLineWidth(render_state.wireframe_width);
    } else {
        cb.setLineWidth(1.0f);
    }

    // Set dynamic viewport and scissor
    auto extent = film.swapchain_image_size();
    vk::Viewport viewport;
    viewport.setX(0.0f);
    viewport.setY(0.0f);
    viewport.setWidth(static_cast<float>(extent.x));
    viewport.setHeight(static_cast<float>(extent.y));
    viewport.setMinDepth(0.0f);
    viewport.setMaxDepth(1.0f);
    cb.setViewport(0, { viewport });

    vk::Rect2D scissor;
    scissor.setOffset({ 0, 0 });
    scissor.setExtent({ extent.x, extent.y });
    cb.setScissor(0, { scissor });

    // Bind descriptor set (set 0)
    cb.bindDescriptorSets(
      vk::PipelineBindPoint::eGraphics,
      manager.pipeline_layout(),
      0,
      { _descriptor_set },
      {});

    // Bind vertex buffers — only those that are populated
    // Must match the binding numbers used in pipeline creation
    std::vector<vk::Buffer> vbufs;
    std::vector<vk::DeviceSize> offsets;

    // binding 0: positions (always)
    vbufs.push_back(_buffers.positions_buffer());
    offsets.push_back(0);

    if (_buffers.has_normals()) {
        vbufs.push_back(_buffers.normals_buffer());
        offsets.push_back(0);
    }
    if (_buffers.has_colors()) {
        vbufs.push_back(_buffers.colors_buffer());
        offsets.push_back(0);
    }
    if (_buffers.has_scalars()) {
        vbufs.push_back(_buffers.scalars_buffer());
        offsets.push_back(0);
    }

    // First binding index is always 0 for positions.
    // bindVertexBuffers binds contiguous from firstBinding when attribute
    // flags match between pipeline and buffer set.  But we need to bind
    // at the correct binding indices, not contiguously from 0, because
    // there can be gaps (e.g. no normals but has colors at binding 2).
    //
    // Bind each buffer individually at its correct binding index.
    uint32_t binding = 0;
    cb.bindVertexBuffers(binding, { _buffers.positions_buffer() }, { vk::DeviceSize{ 0 } });
    if (_buffers.has_normals()) {
        cb.bindVertexBuffers(1, { _buffers.normals_buffer() }, { vk::DeviceSize{ 0 } });
    }
    if (_buffers.has_colors()) {
        cb.bindVertexBuffers(2, { _buffers.colors_buffer() }, { vk::DeviceSize{ 0 } });
    }
    if (_buffers.has_scalars()) {
        cb.bindVertexBuffers(3, { _buffers.scalars_buffer() }, { vk::DeviceSize{ 0 } });
    }

    // Issue draw command based on render mode
    switch (render_state.render_mode) {
    case RenderMode::Solid:
    case RenderMode::SolidWireframe:// SolidWireframe falls back to solid for now
        if (_buffers.has_triangle_indices()) {
            cb.bindIndexBuffer(_buffers.triangle_index_buffer(), 0, vk::IndexType::eUint32);
            cb.drawIndexed(_buffers.triangle_count() * 3, 1, 0, 0, 0);
        } else {
            cb.draw(_buffers.vertex_count(), 1, 0, 0);
        }
        break;

    case RenderMode::Wireframe:
        if (_buffers.has_edge_indices()) {
            cb.bindIndexBuffer(_buffers.edge_index_buffer(), 0, vk::IndexType::eUint32);
            cb.drawIndexed(_buffers.edge_count() * 2, 1, 0, 0, 0);
        } else if (_buffers.has_triangle_indices()) {
            // Fallback: draw triangle edges as lines (not ideal, has duplicates)
            cb.bindIndexBuffer(_buffers.triangle_index_buffer(), 0, vk::IndexType::eUint32);
            cb.drawIndexed(_buffers.triangle_count() * 3, 1, 0, 0, 0);
        } else {
            cb.draw(_buffers.vertex_count(), 1, 0, 0);
        }
        break;

    case RenderMode::Points:
        cb.draw(_buffers.vertex_count(), 1, 0, 0);
        break;
    }
}

// ── release ──────────────────────────────────────────────────────────

void MeshDrawable::release() {
    _buffers.release();
    _transform_ubo.release();
    _material_ubo.release();
    // Descriptor set is freed when the pool is destroyed (by MeshPipelineManager).
    _descriptor_set = vk::DescriptorSet{};
    _initialized = false;
}

}// namespace balsa::visualization::vulkan
