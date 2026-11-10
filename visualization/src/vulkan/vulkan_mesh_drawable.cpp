#include "balsa/visualization/vulkan/vulkan_mesh_drawable.hpp"
#include "balsa/scene_graph/BasicShading.hpp"
#include "balsa/scene_graph/MeshGeometry.hpp"
#include "balsa/scene_graph/Object.hpp"
#include "balsa/scene_graph/types.hpp"
#include "balsa/visualization/vulkan/film.hpp"
#include "balsa/visualization/vulkan/mesh_pipeline.hpp"

#include <algorithm>
#include <cstring>

#include <spdlog/spdlog.h>

namespace balsa::visualization::vulkan {

namespace {
    auto host_buffer(Film &film, vk::DeviceSize size) -> VulkanBuffer {
        return VulkanBuffer(film, size, vk::BufferUsageFlagBits::eUniformBuffer,
                            vk::MemoryPropertyFlagBits::eHostVisible
                                | vk::MemoryPropertyFlagBits::eHostCoherent);
    }

    auto topology_of(scene_graph::MeshPrimitive primitive) -> vk::PrimitiveTopology {
        switch (primitive) {
        case scene_graph::MeshPrimitive::Triangles: return vk::PrimitiveTopology::eTriangleList;
        case scene_graph::MeshPrimitive::Edges: return vk::PrimitiveTopology::eLineList;
        case scene_graph::MeshPrimitive::Vertices: break;
        }
        return vk::PrimitiveTopology::ePointList;
    }
} // namespace

// ── Constructor / Destructor ────────────────────────────────────────

VulkanMeshDrawable::VulkanMeshDrawable(scene_graph::DrawableGroup &group,
                                       MeshPipelineManager &manager)
  : VulkanDrawable(group), _manager(&manager) {}

VulkanMeshDrawable::~VulkanMeshDrawable() { release(); }

// ── Lifecycle ───────────────────────────────────────────────────────

void VulkanMeshDrawable::init(Film &film) {
    if (_initialized) return;

    // Material blocks sit at aligned offsets for dynamic binding.
    const auto alignment = film.physical_device_properties().limits.minUniformBufferOffsetAlignment;
    const vk::DeviceSize block = shaders::max_material_size;
    _material_stride = alignment == 0 ? block : (block + alignment - 1) / alignment * alignment;

    _frames.resize(static_cast<std::size_t>(film.concurrent_frame_count()));
    for (auto &frame : _frames) {
        frame.transforms = host_buffer(film, sizeof(TransformUBO));
        frame.lighting = host_buffer(film, sizeof(SceneLightingUBO));
        frame.materials = host_buffer(film, _material_stride * k_max_material_layers);
        frame.scene_set = _manager->allocate_scene_set();
        _manager->write_scene_set(frame.scene_set, frame.transforms.buffer(),
                                  sizeof(TransformUBO), frame.lighting.buffer(),
                                  sizeof(SceneLightingUBO));
    }
    _initialized = true;
}

void VulkanMeshDrawable::release() {
    for (auto &view : _view_buffers) view->buffers.release();
    _view_buffers.clear();

    // The caller ensures the GPU no longer uses these (e.g.
    // MeshScene::remove_mesh() waits for the device first).
    if (_manager) {
        for (auto &frame : _frames) {
            _manager->free_scene_set(frame.scene_set);
            for (const auto &entry : frame.material_sets) {
                if (entry.set) {
                    _manager->free_material_set(*shaders::shader_programs().find(entry.program),
                                                entry.set);
                }
            }
        }
    }
    _frames.clear();
    _initialized = false;
}

// ── VulkanDrawable interface ────────────────────────────────────────

void VulkanMeshDrawable::draw(const scene_graph::Camera &cam, Film &film) {
    if (!_initialized) return;
    if (!object().visible_in_hierarchy()) return;
    update_scene_ubos(cam, film);
    record_draw_commands(film);
}

// ── Private: sync ───────────────────────────────────────────────────

auto VulkanMeshDrawable::buffers_for(Film &film,
                                     const scene_graph::BasicShading &shading,
                                     const scene_graph::MeshRenderLayer &layer)
    -> MeshBuffers * {
    const auto geometry = shading.layer_geometry(layer);
    if (!geometry) return nullptr;
    auto it = std::ranges::find(_view_buffers, layer.view,
                                [](const auto &v) { return v->view; });
    if (it == _view_buffers.end()) {
        _view_buffers.push_back(std::make_unique<ViewBuffers>(layer.view));
        it = std::prev(_view_buffers.end());
    }
    auto &view = **it;
    if (view.geometry == geometry && view.version == geometry->version()) {
        return &view.buffers;
    }

    // Attribute values may have changed in place; layers re-upload what
    // they bind on their next draw.
    view.buffers.clear_attributes();
    if (geometry->has_triangle_indices()) {
        // Column per triangle, so the buffer is element-major.
        const auto triangles = geometry->triangle_indices();
        view.buffers.upload_triangle_indices(
            film,
            std::span(triangles.data_handle(), triangles.size()),
            static_cast<uint32_t>(triangles.extent(1)));
    }
    if (geometry->has_edge_indices()) {
        const auto edges = geometry->edge_indices();
        view.buffers.upload_edge_indices(
            film,
            std::span(edges.data_handle(), edges.size()),
            static_cast<uint32_t>(edges.extent(1)));
    }
    if (geometry->has_simplices(0)) {
        const auto points = geometry->simplices<0>();
        view.buffers.upload_point_indices(
            film, std::span(points.data_handle(), points.size()));
    }
    view.geometry = geometry;
    view.version = geometry->version();
    return &view.buffers;
}

// ── Private: set 0 ──────────────────────────────────────────────────

void VulkanMeshDrawable::update_scene_ubos(const scene_graph::Camera &cam, Film &film) {
    auto &frame = _frames[static_cast<std::size_t>(film.current_frame())];

    const auto model = object().world_transform();
    const auto camera_position = cam.object().world_transform().translation();
    TransformUBO transform;
    transform.model = model.to_matrix();
    transform.view = cam.view_matrix();
    transform.projection = cam.projection_matrix();
    transform.normal_matrix = model.inverse().to_matrix().transpose().eval();
    transform.camera_pos = scene_graph::Vec4f{static_cast<float>(camera_position(0)),
                                              static_cast<float>(camera_position(1)),
                                              static_cast<float>(camera_position(2)), 0.0f};
    frame.transforms.upload(&transform, sizeof(TransformUBO));

    const auto &light = _scene_light_state;
    const SceneLightingUBO lighting{
        .light_direction = {light.light_dir(0), light.light_dir(1), light.light_dir(2), 0.0f},
        .light_color = {light.light_color(0), light.light_color(1), light.light_color(2),
                        light.enabled ? 1.0f : 0.0f}};
    frame.lighting.upload(&lighting, sizeof(SceneLightingUBO));
}

// ── Private: set 1 ──────────────────────────────────────────────────

auto VulkanMeshDrawable::material_set(Film &film,
                                      uint32_t layer_slot,
                                      const shaders::ShaderProgram &program,
                                      const scene_graph::MeshMaterial &material)
    -> vk::DescriptorSet {
    namespace sg = scene_graph;
    const auto &slots = program.value().textures;
    std::vector<const sg::Texture *> textures(slots.size(), nullptr);
    for (std::size_t i = 0; i < slots.size() && i < material.textures.size(); ++i) {
        const auto &t = material.textures[i];
        if (t && t->texture && t->texture->image) textures[i] = t->texture.get();
    }

    auto &frame = _frames[static_cast<std::size_t>(film.current_frame())];
    auto &entry = frame.material_sets[layer_slot];
    // This frame slot's previous use has completed, so the set may be
    // freed or rewritten.
    if (entry.program != program.id()) {
        if (entry.set) {
            _manager->free_material_set(*shaders::shader_programs().find(entry.program), entry.set);
        }
        entry = {.set = _manager->allocate_material_set(program),
                 .program = std::string(program.id())};
    } else if (entry.textures == textures) {
        return entry.set;
    }

    auto &cache = _manager->texture_cache();
    std::vector<vk::DescriptorImageInfo> infos;
    for (std::size_t i = 0; i < slots.size(); ++i) {
        const SampledImage *image = nullptr;
        vk::Sampler sampler;
        if (const auto *texture = textures[i]) {
            image = &cache.image(film, texture->image, slots[i].color_space);
            sampler = cache.sampler(film, texture->sampler);
        } else {
            image = slots[i].fallback == shaders::TextureFallback::FlatNormal ? &cache.flat_normal(film)
                                                                              : &cache.white(film);
            sampler = cache.sampler(film, sg::Sampler{});
        }
        infos.push_back({sampler, image->view(), vk::ImageLayout::eShaderReadOnlyOptimal});
    }
    _manager->write_material_set(program, entry.set, frame.materials.buffer(), infos);
    entry.textures = std::move(textures);
    return entry.set;
}

// ── Private: draw ───────────────────────────────────────────────────

void VulkanMeshDrawable::record_draw_commands(Film &film) {
    const auto *shading = object().find_feature<scene_graph::BasicShading>();
    if (!shading) return;

    auto cb = film.current_command_buffer();
    auto &frame = _frames[static_cast<std::size_t>(film.current_frame())];
    const auto extent = film.swapchain_image_size();
    const vk::Viewport viewport{0.0f, 0.0f, static_cast<float>(extent[0]),
                                static_cast<float>(extent[1]), 0.0f, 1.0f};
    const vk::Rect2D scissor{{0, 0}, {extent[0], extent[1]}};

    uint32_t layer_slot = 0;
    for (const auto &layer : shading->render_layers()) {
        if (!layer.enabled || !layer.renderer) continue;
        if (layer_slot == k_max_material_layers) break;
        auto *buffers = buffers_for(film, *shading, layer);
        if (buffers == nullptr) continue;

        auto request = layer.renderer->draw_request();
        const auto &program = request.program.value();
        const bool indexed_available =
            (request.primitive == scene_graph::MeshPrimitive::Triangles && buffers->has_triangle_indices())
            || (request.primitive == scene_graph::MeshPrimitive::Edges && buffers->has_edge_indices())
            || request.primitive == scene_graph::MeshPrimitive::Vertices;
        if (!indexed_available) continue;

        // The program's streams from the layer's role bindings; optional
        // ones that are bound enable their definition.
        std::array<const AttributeBuffer *, max_vertex_streams> streams{};
        std::array<vk::Format, max_vertex_streams> formats{};
        bool complete = true;
        for (const auto &stream : program.streams) {
            const auto *buffer =
                buffers->attribute(film, shading->layer_attribute(layer, stream.role));
            if (buffer == nullptr) {
                complete = complete && !stream.required;
                continue;
            }
            streams[stream.location] = buffer;
            formats[stream.location] = buffer->format;
            if (!stream.define.empty()) request.variant.defines.push_back(stream.define);
        }
        if (!complete || streams[0] == nullptr) continue;

        scene_graph::MeshMaterial material;
        layer.renderer->write_material(material, scene_graph::MeshLayerContext(*shading, layer.id));
        if (material.uniform.size() != program.material_size) {
            spdlog::error("VulkanMeshDrawable: layer \"{}\" wrote {} material bytes; {} needs {}",
                          layer.name, material.uniform.size(), program.label,
                          program.material_size);
            continue;
        }

        const auto pipeline = _manager->pipeline(_manager->make_key(
            request.program, std::move(request.variant), formats, topology_of(request.primitive),
            request.cull_mode, request.depth_write));
        if (!pipeline) continue;

        const auto offset = static_cast<vk::DeviceSize>(layer_slot) * _material_stride;
        frame.materials.upload(material.uniform.data(), material.uniform.size(), offset);
        const auto layout = _manager->pipeline_layout(request.program);
        const auto set = material_set(film, layer_slot, request.program, material);

        cb.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline);
        cb.setViewport(0, viewport);
        cb.setScissor(0, scissor);
        cb.setLineWidth(layer.renderer->line_width());
        cb.setDepthBias(0.0f, 0.0f, 0.0f);
        cb.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, layout, 0,
                              {frame.scene_set, set}, {static_cast<uint32_t>(offset)});
        for (uint32_t location = 0; location < streams.size(); ++location) {
            if (const auto *buffer = streams[location]) {
                cb.bindVertexBuffers(location, {buffer->buffer.buffer()}, {vk::DeviceSize{0}});
            }
        }
        if (request.primitive == scene_graph::MeshPrimitive::Triangles) {
            cb.bindIndexBuffer(buffers->triangle_index_buffer(), 0, vk::IndexType::eUint32);
            cb.drawIndexed(buffers->triangle_count() * 3, 1, 0, 0, 0);
        } else if (request.primitive == scene_graph::MeshPrimitive::Edges) {
            cb.bindIndexBuffer(buffers->edge_index_buffer(), 0, vk::IndexType::eUint32);
            cb.drawIndexed(buffers->edge_count() * 2, 1, 0, 0, 0);
        } else if (buffers->has_point_indices()) {
            cb.bindIndexBuffer(buffers->point_index_buffer(), 0, vk::IndexType::eUint32);
            cb.drawIndexed(buffers->point_count(), 1, 0, 0, 0);
        } else {
            cb.draw(streams[0]->count, 1, 0, 0);
        }
        ++layer_slot;
    }

    // Retire buffers of views no layer references any more.
    std::erase_if(_view_buffers, [&](auto &view) {
        if (std::ranges::contains(shading->render_layers(), view->view,
                                  &scene_graph::MeshRenderLayer::view)) {
            return false;
        }
        film.retire(std::move(view->buffers));
        return true;
    });
}

} // namespace balsa::visualization::vulkan
