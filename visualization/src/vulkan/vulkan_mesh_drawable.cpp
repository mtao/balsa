#include "balsa/visualization/vulkan/vulkan_mesh_drawable.hpp"
#include "balsa/visualization/vulkan/film.hpp"
#include "balsa/visualization/vulkan/mesh_pipeline.hpp"
#include "balsa/scene_graph/MeshData.hpp"
#include "balsa/scene_graph/Object.hpp"
#include "balsa/scene_graph/types.hpp"

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

    // Upload transforms once per frame.
    update_transform_ubo(cam);

    // Record multi-layer draw commands.
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

// ── Private: transform UBO ──────────────────────────────────────────

void VulkanMeshDrawable::update_transform_ubo(const scene_graph::Camera &cam) {
    if (!_initialized) return;

    // Model matrix from the scene graph Object's world transform.
    auto model_affine = object().world_transform();
    auto model = model_affine.to_matrix();
    auto view = cam.view_matrix();
    auto projection = cam.projection_matrix();

    // Inverse-transpose of model (for transforming normals).
    scene_graph::Mat4f normal_matrix = model_affine.inverse().to_matrix().transpose().eval();

    TransformUBO transform;
    transform.model = model;
    transform.view = view;
    transform.projection = projection;
    transform.normal_matrix = normal_matrix;
    _transform_ubo.upload(&transform, sizeof(TransformUBO));
}

// ── Private: per-layer material UBO ─────────────────────────────────

void VulkanMeshDrawable::upload_material_ubo_for_layer(
  const float layer_color[4],
  float point_size,
  const MeshRenderState &rs) {

    MaterialUBO material;

    // Uniform color (used by COLOR_UNIFORM path in the shader).
    material.uniform_color(0) = rs.uniform_color[0];
    material.uniform_color(1) = rs.uniform_color[1];
    material.uniform_color(2) = rs.uniform_color[2];
    material.uniform_color(3) = rs.uniform_color[3];

    // Lighting — scene light direction + per-mesh material response.
    material.light_dir(0) = _scene_light_state.light_dir[0];
    material.light_dir(1) = _scene_light_state.light_dir[1];
    material.light_dir(2) = _scene_light_state.light_dir[2];
    material.light_dir(3) = rs.material.ambient_strength;

    material.specular_params(0) = rs.material.specular_strength;
    material.specular_params(1) = rs.material.specular_strength;
    material.specular_params(2) = rs.material.specular_strength;
    material.specular_params(3) = rs.material.shininess;

    // Scalar / sizing parameters.
    material.scalar_params(0) = rs.scalar_min;
    material.scalar_params(1) = rs.scalar_max;
    material.scalar_params(2) = point_size;
    material.scalar_params(3) = rs.two_sided ? 1.0f : 0.0f;

    // Per-layer colour (consumed by RENDER_WIREFRAME / RENDER_POINTS
    // path in mesh.frag as u_layer_color).
    material.layer_color(0) = layer_color[0];
    material.layer_color(1) = layer_color[1];
    material.layer_color(2) = layer_color[2];
    material.layer_color(3) = layer_color[3];

    _material_ubo.upload(&material, sizeof(MaterialUBO));
}

// ── Private: multi-layer draw commands ──────────────────────────────

void VulkanMeshDrawable::record_draw_commands(Film &film) {
    auto *mesh_data = object().find_feature<scene_graph::MeshData>();
    const auto &rs = mesh_data ? mesh_data->render_state()
                               : MeshRenderState{};
    const auto &layers = rs.layers;

    auto cb = film.current_command_buffer();

    // ── Shared state: viewport, scissor, descriptor set, vertex buffers ──

    auto extent = film.swapchain_image_size();
    vk::Viewport viewport;
    viewport.setX(0.0f);
    viewport.setY(0.0f);
    viewport.setWidth(static_cast<float>(extent[0]));
    viewport.setHeight(static_cast<float>(extent[1]));
    viewport.setMinDepth(0.0f);
    viewport.setMaxDepth(1.0f);

    vk::Rect2D scissor;
    scissor.setOffset({ 0, 0 });
    scissor.setExtent({ extent[0], extent[1] });

    // Bind vertex buffers (shared across all layers).
    auto bind_shared_state = [&](vk::Pipeline pipeline) {
        cb.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline);
        cb.setViewport(0, { viewport });
        cb.setScissor(0, { scissor });

        cb.bindDescriptorSets(
          vk::PipelineBindPoint::eGraphics,
          _manager->pipeline_layout(),
          0,
          { _descriptor_set },
          {});

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
    };

    // ── Layer 1: Solid (triangles) ──────────────────────────────────

    if (layers.solid.enabled && _buffers.has_triangle_indices()) {
        auto pipeline = _manager->get_or_create(
          rs, vk::PrimitiveTopology::eTriangleList, _buffers, film);
        if (pipeline) {
            upload_material_ubo_for_layer(layers.solid.color, layers.points.size, rs);
            bind_shared_state(pipeline);
            cb.setLineWidth(1.0f);
            // Push solid geometry slightly back so wireframe wins z-test.
            cb.setDepthBias(1.0f, 0.0f, 1.0f);

            cb.bindIndexBuffer(_buffers.triangle_index_buffer(), 0, vk::IndexType::eUint32);
            cb.drawIndexed(_buffers.triangle_count() * 3, 1, 0, 0, 0);
        }
    }

    // ── Layer 2: Wireframe (lines) ──────────────────────────────────

    if (layers.wireframe.enabled && _buffers.has_edge_indices()) {
        auto pipeline = _manager->get_or_create(
          rs, vk::PrimitiveTopology::eLineList, _buffers, film);
        if (pipeline) {
            upload_material_ubo_for_layer(layers.wireframe.color, layers.points.size, rs);
            bind_shared_state(pipeline);
            cb.setLineWidth(layers.wireframe.width);
            cb.setDepthBias(0.0f, 0.0f, 0.0f);

            cb.bindIndexBuffer(_buffers.edge_index_buffer(), 0, vk::IndexType::eUint32);
            cb.drawIndexed(_buffers.edge_count() * 2, 1, 0, 0, 0);
        }
    }

    // ── Layer 3: Points ─────────────────────────────────────────────

    if (layers.points.enabled && _buffers.vertex_count() > 0) {
        auto pipeline = _manager->get_or_create(
          rs, vk::PrimitiveTopology::ePointList, _buffers, film);
        if (pipeline) {
            upload_material_ubo_for_layer(layers.points.color, layers.points.size, rs);
            bind_shared_state(pipeline);
            cb.setLineWidth(1.0f);
            cb.setDepthBias(0.0f, 0.0f, 0.0f);

            cb.draw(_buffers.vertex_count(), 1, 0, 0);
        }
    }
}

}// namespace balsa::visualization::vulkan
