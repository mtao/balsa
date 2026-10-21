#include "balsa/visualization/vulkan/vulkan_mesh_drawable.hpp"
#include "balsa/scene_graph/BasicShading.hpp"
#include "balsa/scene_graph/MeshGeometry.hpp"
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

    _synced_geometry = nullptr;
    _synced_geometry_version = 0;
    _initialized = false;
}

// ── VulkanDrawable interface ────────────────────────────────────────

void VulkanMeshDrawable::draw(const scene_graph::Camera &cam, Film &film) {
    if (!_initialized) return;
    if (!object().visible) return;

    // Re-upload GPU buffers if the geometry changed.
    sync_buffers(film);

    // Upload transforms once per frame (to current frame's UBO).
    update_transform_ubo(cam, film);

    // Record multi-layer draw commands.
    record_draw_commands(film);
}

// ── Private: sync ───────────────────────────────────────────────────

void VulkanMeshDrawable::sync_buffers(Film &film) {
    const auto *shading = object().find_feature<scene_graph::BasicShading>();
    const auto *geometry = shading ? shading->geometry() : nullptr;
    if (!geometry) return;
    if (geometry == _synced_geometry
        && geometry->version() == _synced_geometry_version) {
        return;
    }

    // Attribute values may have changed in place; layers re-upload what
    // they bind on their next draw.
    _buffers.clear_attributes();

    // Upload triangle indices.
    if (geometry->has_triangle_indices()) {
        // Column per triangle, so the buffer is element-major.
        const auto triangles = geometry->triangle_indices();
        _buffers.upload_triangle_indices(
            film,
            std::span(triangles.data_handle(), triangles.size()),
            static_cast<uint32_t>(triangles.extent(1)));
    }

    // Upload edge indices.
    if (geometry->has_edge_indices()) {
        const auto edges = geometry->edge_indices();
        _buffers.upload_edge_indices(
            film,
            std::span(edges.data_handle(), edges.size()),
            static_cast<uint32_t>(edges.extent(1)));
    }

    _synced_geometry = geometry;
    _synced_geometry_version = geometry->version();
}

auto VulkanMeshDrawable::layer_inputs(Film &film,
                                      const scene_graph::BasicShading &shading,
                                      const scene_graph::MeshRenderLayer &layer)
    -> MeshVertexInputs {
    auto input = [&](std::string_view role) {
        return _buffers.attribute(film, shading.layer_attribute(layer, role));
    };
    return {.positions = input(scene_graph::position_role),
            .normals = input(scene_graph::normal_role),
            .scalars = input(scene_graph::scalar_role)};
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
    transform.camera_pos = scene_graph::Vec4f{
        static_cast<float>(cam_translation(0)),
        static_cast<float>(cam_translation(1)),
        static_cast<float>(cam_translation(2)),
        0.0f };// w = pad
    _transform_ubos[fi].upload(&transform, sizeof(TransformUBO));
}

// ── Private: per-layer material UBO ─────────────────────────────────

void VulkanMeshDrawable::upload_material_ubo_for_layer(
    Film &film,
    uint32_t layer_slot,
    const MeshRenderState &rs,
    const scene_graph::MeshRenderer &renderer,
    const scene_graph::MeshLayerContext &layer,
    const MeshVertexInputs &inputs) {
    const int fi = film.current_frame();

    MaterialUBO material;

    scene_graph::MeshMaterial renderer_material;
    renderer.write_material(renderer_material, layer);
    material.uniform_color = renderer_material.uniform_color;
    material.layer_color = renderer_material.layer_color;

    // With no enabled scene light, preserve the base color as an unlit pass.
    // Otherwise disabling the headlight would accidentally fall back to the
    // ResolvedLightState's default white light.
    const bool lighting_enabled = _scene_light_state.enabled;

    // Lighting — scene light direction + per-mesh material response.
    const auto &ld = _scene_light_state.light_dir;
    material.light_dir =
        scene_graph::Vec4f{ ld(0), ld(1), ld(2),
                            lighting_enabled ? rs.material.ambient_strength : 1.0f };

    // Light color (from scene light) + diffuse strength in .w.
    const auto &lcol = _scene_light_state.light_color;
    material.light_color = scene_graph::Vec4f{
        lcol(0), lcol(1), lcol(2),
        lighting_enabled ? rs.material.diffuse_strength : 0.0f };

    material.specular_params = scene_graph::Vec4f{
        lighting_enabled ? rs.material.specular_strength : 0.0f,
        lighting_enabled ? rs.material.specular_strength : 0.0f,
        lighting_enabled ? rs.material.specular_strength : 0.0f,
        rs.material.shininess };

    // Scalar / sizing parameters.
    material.scalar_params(0) = renderer_material.scalar_min;
    material.scalar_params(1) = renderer_material.scalar_max;
    material.scalar_params(2) = renderer_material.primitive_size;
    material.scalar_params(3) = rs.two_sided ? 1.0f : 0.0f;

    // Reduction of the raw scalar-role attribute, evaluated in the shader.
    const auto &reduction = renderer_material.scalar_reduction;
    material.scalar_select = scene_graph::Vec4f{
        reduction.kind == scene_graph::ScalarReduction::Kind::Norm ? 1.0f : 0.0f,
        static_cast<float>(reduction.value),
        static_cast<float>(inputs.scalars ? inputs.scalars->components : 0),
        0.0f};

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
    const auto *shading = object().find_feature<scene_graph::BasicShading>();
    if (!shading) return;

    auto cb = film.current_command_buffer();

    // ── Shared state: viewport, scissor ─────────────────────────────
    // Pipeline, vertex streams and the descriptor set (dynamic material
    // offset) are bound per layer.

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

    // Each layer binds its own vertex streams: layers may bind different
    // attributes to the same role.
    auto bind_vertex_inputs = [&](const MeshVertexInputs &inputs) {
        auto bind = [&](uint32_t binding, const AttributeBuffer *b) {
            if (b) cb.bindVertexBuffers(binding, {b->buffer.buffer()}, {vk::DeviceSize{0}});
        };
        bind(0, inputs.positions);
        bind(1, inputs.normals);
        bind(3, inputs.scalars);
    };

    uint32_t layer_slot = 0;
    for (const auto &layer : shading->render_layers()) {
        if (!layer.enabled || !layer.renderer) continue;
        if (layer_slot == k_max_material_layers) break;
        const auto inputs = layer_inputs(film, *shading, layer);
        if (inputs.positions == nullptr) continue;
        const auto request = layer.renderer->draw_request();
        const auto topology = request.primitive == scene_graph::MeshPrimitive::Triangles
                                  ? vk::PrimitiveTopology::eTriangleList
                              : request.primitive == scene_graph::MeshPrimitive::Edges
                                  ? vk::PrimitiveTopology::eLineList
                                  : vk::PrimitiveTopology::ePointList;
        const bool available =
            (request.primitive == scene_graph::MeshPrimitive::Triangles && _buffers.has_triangle_indices())
            || (request.primitive == scene_graph::MeshPrimitive::Edges && _buffers.has_edge_indices())
            || request.primitive == scene_graph::MeshPrimitive::Vertices;
        if (!available) continue;
        const auto pipeline = _manager->get_or_create(request.state, request.colormap, topology,
                                                      inputs, film);
        if (!pipeline) continue;

        upload_material_ubo_for_layer(film, layer_slot, request.state,
                                      *layer.renderer,
                                      scene_graph::MeshLayerContext(*shading, layer.id),
                                      inputs);
        cb.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline);
        cb.setViewport(0, {viewport});
        cb.setScissor(0, {scissor});
        bind_vertex_inputs(inputs);
        bind_descriptor_set_for_layer(film, cb, layer_slot);
        cb.setLineWidth(layer.renderer->line_width());
        cb.setDepthBias(0.0f, 0.0f, 0.0f);
        if (request.primitive == scene_graph::MeshPrimitive::Triangles) {
            cb.bindIndexBuffer(_buffers.triangle_index_buffer(), 0, vk::IndexType::eUint32);
            cb.drawIndexed(_buffers.triangle_count() * 3, 1, 0, 0, 0);
        } else if (request.primitive == scene_graph::MeshPrimitive::Edges) {
            cb.bindIndexBuffer(_buffers.edge_index_buffer(), 0, vk::IndexType::eUint32);
            cb.drawIndexed(_buffers.edge_count() * 2, 1, 0, 0, 0);
        } else {
            cb.draw(inputs.positions->count, 1, 0, 0);
        }
        ++layer_slot;
    }
}

} // namespace balsa::visualization::vulkan
