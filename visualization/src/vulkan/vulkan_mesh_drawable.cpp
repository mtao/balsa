#include "balsa/visualization/vulkan/vulkan_mesh_drawable.hpp"
#include "balsa/visualization/vulkan/film.hpp"
#include "balsa/visualization/vulkan/mesh_pipeline.hpp"
#include "balsa/scene_graph/MeshData.hpp"
#include "balsa/scene_graph/Object.hpp"
#include "balsa/glm/zipper_compat.hpp"

#include <spdlog/spdlog.h>

namespace balsa::visualization::vulkan {

// ── Constructor / Destructor ────────────────────────────────────────

VulkanMeshDrawable::VulkanMeshDrawable(scene_graph::DrawableGroup &group,
                                       MeshPipelineManager &manager)
  : VulkanDrawable(group),
    _manager(&manager) {
}

VulkanMeshDrawable::~VulkanMeshDrawable() {
    release();
}

// ── Lifecycle ───────────────────────────────────────────────────────

void VulkanMeshDrawable::init(Film &film) {
    if (_initialized) return;

    // Create host-visible UBO buffers.
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

    // Allocate and write descriptor set.
    _descriptor_set = _manager->allocate_descriptor_set();
    _manager->write_descriptor_set(
      _descriptor_set,
      _transform_ubo.buffer(),
      sizeof(TransformUBO),
      _material_ubo.buffer(),
      sizeof(MaterialUBO));

    _initialized = true;
}

void VulkanMeshDrawable::release() {
    _buffers.release();
    _transform_ubo.release();
    _material_ubo.release();
    _descriptor_set = vk::DescriptorSet{};
    _synced_version = 0;
    _initialized = false;
}

// ── VulkanDrawable interface ────────────────────────────────────────

void VulkanMeshDrawable::draw(const scene_graph::Camera &cam, Film &film) {
    if (!_initialized) return;
    if (!object().visible) return;

    // Sync GPU buffers from MeshData if dirty.
    sync_from_mesh_data(film);

    if (!_buffers.has_positions()) return;

    // Update UBOs with current transforms and material.
    update_ubos(cam);

    // Record draw commands.
    record_draw_commands(film);
}

// ── Private: sync ───────────────────────────────────────────────────

void VulkanMeshDrawable::sync_from_mesh_data(Film &film) {
    auto *mesh_data = object().find_feature<scene_graph::MeshData>();
    if (!mesh_data) return;
    if (mesh_data->version() == _synced_version) return;

    // Upload positions — reinterpret Vec3f[] as contiguous float[].
    if (mesh_data->has_positions()) {
        auto positions = mesh_data->positions();
        auto float_span = std::span<const float>(
          reinterpret_cast<const float *>(positions.data()),
          positions.size() * 3);
        _buffers.upload_positions(
          film, float_span, static_cast<uint32_t>(positions.size()));
    }

    // Upload normals.
    if (mesh_data->has_normals()) {
        auto normals = mesh_data->normals();
        auto float_span = std::span<const float>(
          reinterpret_cast<const float *>(normals.data()),
          normals.size() * 3);
        _buffers.upload_normals(film, float_span);
    }

    // Upload per-vertex colors (Vec4f → 4 floats, but MeshBuffers
    // expects 3 floats per vertex for colors).  We strip alpha here.
    if (mesh_data->has_vertex_colors()) {
        auto colors = mesh_data->vertex_colors();
        std::vector<float> rgb(colors.size() * 3);
        for (std::size_t i = 0; i < colors.size(); ++i) {
            rgb[i * 3 + 0] = colors[i](0);
            rgb[i * 3 + 1] = colors[i](1);
            rgb[i * 3 + 2] = colors[i](2);
        }
        _buffers.upload_colors(film, rgb);
    }

    // Upload scalar field.
    if (mesh_data->has_scalar_field()) {
        _buffers.upload_scalars(film, mesh_data->scalar_field());
    }

    // Upload triangle indices.
    if (mesh_data->has_triangle_indices()) {
        auto tri_idx = mesh_data->triangle_indices();
        _buffers.upload_triangle_indices(
          film, tri_idx, static_cast<uint32_t>(tri_idx.size() / 3));
    }

    // Upload edge indices.
    if (mesh_data->has_edge_indices()) {
        auto edge_idx = mesh_data->edge_indices();
        _buffers.upload_edge_indices(
          film, edge_idx, static_cast<uint32_t>(edge_idx.size() / 2));
    }

    _synced_version = mesh_data->version();
}

// ── Private: UBO update ─────────────────────────────────────────────

void VulkanMeshDrawable::update_ubos(const scene_graph::Camera &cam) {
    if (!_initialized) return;

    auto *mesh_data = object().find_feature<scene_graph::MeshData>();

    // Model matrix from the scene graph Object's world transform.
    auto model_zipper = object().world_transform();
    auto model = glm_compat::as_glm(model_zipper);
    auto view = glm_compat::as_glm(cam.view_matrix());
    auto projection = glm_compat::as_glm(cam.projection_matrix());

    // TransformUBO
    TransformUBO transform;
    transform.model = model;
    transform.view = view;
    transform.projection = projection;
    transform.normal_matrix = glm_compat::as_glm(
      glm_compat::inverse_transpose(model_zipper));
    _transform_ubo.upload(&transform, sizeof(TransformUBO));

    // MaterialUBO — pack from MeshData's render_state.
    const auto &rs = mesh_data ? mesh_data->render_state()
                               : MeshRenderState{};

    MaterialUBO material;
    material.uniform_color = glm::vec4(
      rs.uniform_color[0],
      rs.uniform_color[1],
      rs.uniform_color[2],
      rs.uniform_color[3]);
    material.light_dir = glm::vec4(
      rs.light_dir[0],
      rs.light_dir[1],
      rs.light_dir[2],
      rs.ambient_strength);
    material.specular_params = glm::vec4(
      rs.specular_strength,
      rs.specular_strength,
      rs.specular_strength,
      rs.shininess);
    material.scalar_params = glm::vec4(
      rs.scalar_min,
      rs.scalar_max,
      rs.point_size,
      rs.two_sided ? 1.0f : 0.0f);
    material.wireframe_color = glm::vec4(
      rs.wireframe_color[0],
      rs.wireframe_color[1],
      rs.wireframe_color[2],
      rs.wireframe_color[3]);
    _material_ubo.upload(&material, sizeof(MaterialUBO));
}

// ── Private: draw commands ──────────────────────────────────────────

void VulkanMeshDrawable::record_draw_commands(Film &film) {
    auto *mesh_data = object().find_feature<scene_graph::MeshData>();
    const auto &rs = mesh_data ? mesh_data->render_state()
                               : MeshRenderState{};

    // Get (or create) the pipeline for the current state.
    auto pipeline = _manager->get_or_create(rs, _buffers, film);
    if (!pipeline) return;

    auto cb = film.current_command_buffer();

    // Bind pipeline.
    cb.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline);

    // Dynamic line width.
    if (rs.render_mode == RenderMode::Wireframe || rs.render_mode == RenderMode::SolidWireframe) {
        cb.setLineWidth(rs.wireframe_width);
    } else {
        cb.setLineWidth(1.0f);
    }

    // Dynamic viewport and scissor.
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

    // Bind descriptor set (set 0).
    cb.bindDescriptorSets(
      vk::PipelineBindPoint::eGraphics,
      _manager->pipeline_layout(),
      0,
      { _descriptor_set },
      {});

    // Bind vertex buffers at their correct binding indices.
    cb.bindVertexBuffers(0, { _buffers.positions_buffer() }, { vk::DeviceSize{ 0 } });
    if (_buffers.has_normals()) {
        cb.bindVertexBuffers(1, { _buffers.normals_buffer() }, { vk::DeviceSize{ 0 } });
    }
    if (_buffers.has_colors()) {
        cb.bindVertexBuffers(2, { _buffers.colors_buffer() }, { vk::DeviceSize{ 0 } });
    }
    if (_buffers.has_scalars()) {
        cb.bindVertexBuffers(3, { _buffers.scalars_buffer() }, { vk::DeviceSize{ 0 } });
    }

    // Issue draw command based on render mode.
    switch (rs.render_mode) {
    case RenderMode::Solid:
    case RenderMode::SolidWireframe:
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

}// namespace balsa::visualization::vulkan
