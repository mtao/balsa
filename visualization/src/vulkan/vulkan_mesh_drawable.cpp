#include "balsa/visualization/vulkan/vulkan_mesh_drawable.hpp"
#include "balsa/scene_graph/MeshData.hpp"
#include "balsa/scene_graph/Object.hpp"
#include "balsa/scene_graph/types.hpp"
#include "balsa/visualization/vulkan/film.hpp"
#include "balsa/visualization/vulkan/mesh_pipeline.hpp"

#include <spdlog/spdlog.h>

namespace balsa::visualization::vulkan {

// ── Constructor / Destructor ────────────────────────────────────────

VulkanMeshDrawable::VulkanMeshDrawable(scene_graph::DrawableGroup &group,
                                       MeshPipelineManager &manager)
  : VulkanDrawable(group), _manager(&manager) {}

VulkanMeshDrawable::~VulkanMeshDrawable() { release(); }

// ── Lifecycle ───────────────────────────────────────────────────────

void VulkanMeshDrawable::init(Film &film) {
    if (_initialized) return;

    const int frame_count = film.concurrent_frame_count();

    // Material UBO alignment — shared across all frames.
    auto min_align = film.physical_device_properties()
                         .limits.minUniformBufferOffsetAlignment;
    _material_ubo_stride = material_ubo_aligned_size(min_align);

    _transform_ubos.resize(frame_count);
    _material_ubos.resize(frame_count);
    _descriptor_sets.resize(frame_count);

    for (int i = 0; i < frame_count; ++i) {
        // Create host-visible UBO buffers.
        _transform_ubos[i] =
            VulkanBuffer(film,
                         sizeof(TransformUBO),
                         vk::BufferUsageFlagBits::eUniformBuffer,
                         vk::MemoryPropertyFlagBits::eHostVisible
                             | vk::MemoryPropertyFlagBits::eHostCoherent);

        _material_ubos[i] =
            VulkanBuffer(film,
                         _material_ubo_stride * k_max_material_layers,
                         vk::BufferUsageFlagBits::eUniformBuffer,
                         vk::MemoryPropertyFlagBits::eHostVisible
                             | vk::MemoryPropertyFlagBits::eHostCoherent);

        // Allocate and write descriptor set.  The material range covers
        // a single slot; the dynamic offset selects which slot to read.
        _descriptor_sets[i] = _manager->allocate_descriptor_set();
        _manager->write_descriptor_set(_descriptor_sets[i],
                                       _transform_ubos[i].buffer(),
                                       sizeof(TransformUBO),
                                       _material_ubos[i].buffer(),
                                       sizeof(MaterialUBO));
    }

    _initialized = true;
}

void VulkanMeshDrawable::release() {
    _buffers.release();

    for (auto &ubo : _transform_ubos) ubo.release();
    _transform_ubos.clear();

    for (auto &ubo : _material_ubos) ubo.release();
    _material_ubos.clear();

    // Return descriptor sets to the pool so they can be reused (e.g.
    // when BVH overlays are rebuilt repeatedly).  The caller must ensure
    // the GPU is idle before calling release() — MeshScene::remove_mesh()
    // handles this by calling device.waitIdle() first.
    if (_manager) {
        for (auto ds : _descriptor_sets) {
            if (ds) _manager->free_descriptor_set(ds);
        }
    }
    _descriptor_sets.clear();

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

    // Upload transforms once per frame (to current frame's UBO).
    update_transform_ubo(cam, film);

    // Record multi-layer draw commands.
    record_draw_commands(film);
}

// ── Private: sync ───────────────────────────────────────────────────

void VulkanMeshDrawable::sync_from_mesh_data(Film &film) {
    auto *mesh_data = object().find_feature<scene_graph::MeshData>();
    if (!mesh_data) return;
    if (mesh_data->version() == _synced_version) return;

    // Upload positions from the role binding's GPU-ready data.
    const auto &pos = mesh_data->position_binding();
    if (pos.is_bound()) {
        const void *data = pos.raw_data();
        std::size_t byte_size =
            pos.size() * pos.component_count * sizeof(float);
        _buffers.upload_positions(film,
                                  data,
                                  byte_size,
                                  static_cast<uint32_t>(pos.size()),
                                  pos.component_count);
    }

    // Upload normals.
    const auto &nrm = mesh_data->normal_binding();
    if (nrm.is_bound()) {
        const void *data = nrm.raw_data();
        std::size_t byte_size =
            nrm.size() * nrm.component_count * sizeof(float);
        _buffers.upload_normals(film, data, byte_size, nrm.component_count);
    }

    // Upload scalar field.
    const auto &scl = mesh_data->scalar_binding();
    if (scl.is_bound()) {
        const auto *fdata = static_cast<const float *>(scl.raw_data());
        _buffers.upload_scalars(film,
                                std::span<const float>(fdata, scl.size()));
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

void VulkanMeshDrawable::update_transform_ubo(const scene_graph::Camera &cam,
                                              Film &film) {
    if (!_initialized) return;

    const int fi = film.current_frame();

    // Model matrix from the scene graph Object's world transform.
    auto model_affine = object().world_transform();
    auto model = model_affine.to_matrix();
    auto view = cam.view_matrix();
    auto projection = cam.projection_matrix();

    // Inverse-transpose of model (for transforming normals).
    scene_graph::Mat4f normal_matrix =
        model_affine.inverse().to_matrix().transpose().eval();

    // Camera world-space position (for specular view vector).
    auto cam_world = cam.object().world_transform();
    auto cam_translation = cam_world.translation();

    TransformUBO transform;
    transform.model = model;
    transform.view = view;
    transform.projection = projection;
    transform.normal_matrix = normal_matrix;
    transform.camera_pos(0) = static_cast<float>(cam_translation(0));
    transform.camera_pos(1) = static_cast<float>(cam_translation(1));
    transform.camera_pos(2) = static_cast<float>(cam_translation(2));
    transform.camera_pos(3) = 0.0f; // pad
    _transform_ubos[fi].upload(&transform, sizeof(TransformUBO));
}

// ── Private: per-layer material UBO ─────────────────────────────────

void VulkanMeshDrawable::upload_material_ubo_for_layer(
    Film &film,
    uint32_t layer_slot,
    const float layer_color[4],
    float point_size,
    const MeshRenderState &rs,
    const float *wireframe_color_override) {
    const int fi = film.current_frame();

    MaterialUBO material;

    // Uniform color (used by COLOR_UNIFORM path in the shader).
    // For the solid layer (slot 0), override with the layer's own color
    // so that each layer independently controls its base color.
    if (layer_slot == 0 && rs.color_source == ColorSource::Uniform) {
        material.uniform_color(0) = layer_color[0];
        material.uniform_color(1) = layer_color[1];
        material.uniform_color(2) = layer_color[2];
        material.uniform_color(3) = layer_color[3];
    } else {
        material.uniform_color(0) = rs.uniform_color[0];
        material.uniform_color(1) = rs.uniform_color[1];
        material.uniform_color(2) = rs.uniform_color[2];
        material.uniform_color(3) = rs.uniform_color[3];
    }

    // Lighting — scene light direction + per-mesh material response.
    material.light_dir(0) = _scene_light_state.light_dir[0];
    material.light_dir(1) = _scene_light_state.light_dir[1];
    material.light_dir(2) = _scene_light_state.light_dir[2];
    material.light_dir(3) = rs.material.ambient_strength;

    // Light color (from scene light) + diffuse strength in .w.
    material.light_color(0) = _scene_light_state.light_color[0];
    material.light_color(1) = _scene_light_state.light_color[1];
    material.light_color(2) = _scene_light_state.light_color[2];
    material.light_color(3) = rs.material.diffuse_strength;

    material.specular_params(0) = rs.material.specular_strength;
    material.specular_params(1) = rs.material.specular_strength;
    material.specular_params(2) = rs.material.specular_strength;
    material.specular_params(3) = rs.material.shininess;

    // Scalar / sizing parameters.
    material.scalar_params(0) = rs.scalar_min;
    material.scalar_params(1) = rs.scalar_max;
    material.scalar_params(2) = point_size;
    material.scalar_params(3) = rs.two_sided ? 1.0f : 0.0f;

    // Per-layer colour.  When wireframe_color_override is provided
    // (merged solid+wireframe pass), use the wireframe color here
    // so the fragment shader can read it from u_layer_color for the
    // wireframe overlay blend.
    const float *lc =
        wireframe_color_override ? wireframe_color_override : layer_color;
    material.layer_color(0) = lc[0];
    material.layer_color(1) = lc[1];
    material.layer_color(2) = lc[2];
    material.layer_color(3) = lc[3];

    // Write to the slot's aligned offset within the multi-slot buffer.
    vk::DeviceSize offset =
        static_cast<vk::DeviceSize>(layer_slot) * _material_ubo_stride;
    _material_ubos[fi].upload(&material, sizeof(MaterialUBO), offset);
}

// ── Private: bind descriptor set with dynamic offset ────────────────

void VulkanMeshDrawable::bind_descriptor_set_for_layer(Film &film,
                                                       vk::CommandBuffer cb,
                                                       uint32_t layer_slot) {
    const int fi = film.current_frame();
    uint32_t dynamic_offset = static_cast<uint32_t>(
        static_cast<vk::DeviceSize>(layer_slot) * _material_ubo_stride);
    cb.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
                          _manager->pipeline_layout(),
                          0,
                          {_descriptor_sets[fi]},
                          {dynamic_offset});
}

// ── Private: multi-layer draw commands ──────────────────────────────

void VulkanMeshDrawable::record_draw_commands(Film &film) {
    auto *mesh_data = object().find_feature<scene_graph::MeshData>();
    const auto &rs = mesh_data ? mesh_data->render_state() : MeshRenderState{};
    const auto &layers = rs.layers;

    auto cb = film.current_command_buffer();

    // ── Shared state: viewport, scissor, vertex buffers ─────────────
    // Note: descriptor set is bound per-layer with a dynamic offset.

    auto extent = film.swapchain_image_size();
    vk::Viewport viewport;
    viewport.setX(0.0f);
    viewport.setY(0.0f);
    viewport.setWidth(static_cast<float>(extent[0]));
    viewport.setHeight(static_cast<float>(extent[1]));
    viewport.setMinDepth(0.0f);
    viewport.setMaxDepth(1.0f);

    vk::Rect2D scissor;
    scissor.setOffset({0, 0});
    scissor.setExtent({extent[0], extent[1]});

    // Bind pipeline + viewport/scissor + vertex buffers (shared across
    // all layers).  Does NOT bind the descriptor set — that happens
    // per-layer via bind_descriptor_set_for_layer().
    auto bind_shared_state = [&](vk::Pipeline pipeline) {
        cb.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline);
        cb.setViewport(0, {viewport});
        cb.setScissor(0, {scissor});

        cb.bindVertexBuffers(
            0, {_buffers.positions_buffer()}, {vk::DeviceSize{0}});
        if (_buffers.has_normals()) {
            cb.bindVertexBuffers(
                1, {_buffers.normals_buffer()}, {vk::DeviceSize{0}});
        }
        if (_buffers.has_colors()) {
            cb.bindVertexBuffers(
                2, {_buffers.colors_buffer()}, {vk::DeviceSize{0}});
        }
        if (_buffers.has_scalars()) {
            cb.bindVertexBuffers(
                3, {_buffers.scalars_buffer()}, {vk::DeviceSize{0}});
        }
    };

    // ── Decide whether to merge solid + wireframe into one draw ────
    //
    // When the VK_KHR_fragment_shader_barycentric extension is available,
    // both solid and wireframe are enabled, and we have triangle data,
    // we can render both in a single eTriangleList draw call.  The
    // fragment shader uses gl_BaryCoordEXT to overlay wireframe edges.
    //
    // Otherwise fall back to the existing two-pass approach: solid
    // triangles + line-based wireframe from the edge index buffer.

    const bool use_merged = layers.solid.enabled && layers.wireframe.enabled
                            && _buffers.has_triangle_indices()
                            && film.has_fragment_shader_barycentric();

    // Upload all enabled layers' material data BEFORE recording any
    // draw commands.  Each layer writes to its own aligned slot in
    // the current frame's _material_ubo, so the writes don't interfere
    // with each other.
    // Layer slots: 0 = solid, 1 = wireframe, 2 = points.
    if (use_merged) {
        // Merged pass: slot 0 carries solid lighting + wireframe overlay.
        //   u_uniform_color   = solid base color (slot 0 override for
        //   COLOR_UNIFORM) u_layer_color     = wireframe color (via
        //   wireframe_color_override) u_scalar_params.z = wireframe width
        //   (passed as point_size)
        upload_material_ubo_for_layer(film,
                                      0,
                                      layers.solid.color,
                                      layers.wireframe.width,
                                      rs,
                                      layers.wireframe.color);
    } else {
        // Separate passes.
        if (layers.solid.enabled && _buffers.has_triangle_indices()) {
            upload_material_ubo_for_layer(
                film, 0, layers.solid.color, layers.points.size, rs);
        }
        if (layers.wireframe.enabled && _buffers.has_edge_indices()) {
            upload_material_ubo_for_layer(
                film, 1, layers.wireframe.color, layers.points.size, rs);
        }
    }
    if (layers.points.enabled && _buffers.vertex_count() > 0) {
        upload_material_ubo_for_layer(
            film, 2, layers.points.color, layers.points.size, rs);
    }

    // ── Merged solid + wireframe (barycentric overlay) ──────────────

    if (use_merged) {
        auto pipeline =
            _manager->get_or_create(rs,
                                    vk::PrimitiveTopology::eTriangleList,
                                    _buffers,
                                    film,
                                    /*wireframe_overlay=*/true);
        if (pipeline) {
            bind_shared_state(pipeline);
            bind_descriptor_set_for_layer(film, cb, 0);
            cb.setLineWidth(1.0f);
            cb.setDepthBias(0.0f, 0.0f, 0.0f);

            cb.bindIndexBuffer(
                _buffers.triangle_index_buffer(), 0, vk::IndexType::eUint32);
            cb.drawIndexed(_buffers.triangle_count() * 3, 1, 0, 0, 0);
        }
    } else {
        // ── Layer 1: Solid (triangles) ──────────────────────────────

        if (layers.solid.enabled && _buffers.has_triangle_indices()) {
            auto pipeline = _manager->get_or_create(
                rs, vk::PrimitiveTopology::eTriangleList, _buffers, film);
            if (pipeline) {
                bind_shared_state(pipeline);
                bind_descriptor_set_for_layer(film, cb, 0);
                cb.setLineWidth(1.0f);
                // Push solid geometry slightly back so wireframe wins
                // z-test, but only when line-based wireframe is also
                // being drawn — the slope bias causes flickering on its
                // own.
                if (layers.wireframe.enabled && _buffers.has_edge_indices()) {
                    cb.setDepthBias(1.0f, 0.0f, 1.0f);
                } else {
                    cb.setDepthBias(0.0f, 0.0f, 0.0f);
                }

                cb.bindIndexBuffer(_buffers.triangle_index_buffer(),
                                   0,
                                   vk::IndexType::eUint32);
                cb.drawIndexed(_buffers.triangle_count() * 3, 1, 0, 0, 0);
            }
        }

        // ── Layer 2: Wireframe (lines) ──────────────────────────────

        if (layers.wireframe.enabled && _buffers.has_edge_indices()) {
            auto pipeline = _manager->get_or_create(
                rs, vk::PrimitiveTopology::eLineList, _buffers, film);
            if (pipeline) {
                bind_shared_state(pipeline);
                bind_descriptor_set_for_layer(film, cb, 1);
                cb.setLineWidth(layers.wireframe.width);
                cb.setDepthBias(0.0f, 0.0f, 0.0f);

                cb.bindIndexBuffer(
                    _buffers.edge_index_buffer(), 0, vk::IndexType::eUint32);
                cb.drawIndexed(_buffers.edge_count() * 2, 1, 0, 0, 0);
            }
        }
    }

    // ── Layer 3: Points ─────────────────────────────────────────────

    if (layers.points.enabled && _buffers.vertex_count() > 0) {
        auto pipeline = _manager->get_or_create(
            rs, vk::PrimitiveTopology::ePointList, _buffers, film);
        if (pipeline) {
            bind_shared_state(pipeline);
            bind_descriptor_set_for_layer(film, cb, 2);
            cb.setLineWidth(1.0f);
            cb.setDepthBias(0.0f, 0.0f, 0.0f);

            cb.draw(_buffers.vertex_count(), 1, 0, 0);
        }
    }
}

} // namespace balsa::visualization::vulkan
