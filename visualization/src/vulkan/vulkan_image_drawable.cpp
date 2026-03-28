#include "balsa/visualization/vulkan/vulkan_image_drawable.hpp"
#include "balsa/scene_graph/ImageData.hpp"
#include "balsa/scene_graph/Object.hpp"
#include "balsa/scene_graph/types.hpp"
#include "balsa/visualization/vulkan/film.hpp"
#include "balsa/visualization/vulkan/image_pipeline.hpp"

#include <spdlog/spdlog.h>

namespace balsa::visualization::vulkan {

// ── Constructor / Destructor ────────────────────────────────────────

VulkanImageDrawable::VulkanImageDrawable(scene_graph::DrawableGroup &group,
                                         ImagePipelineManager &manager)
  : VulkanDrawable(group), _manager(&manager) {}

VulkanImageDrawable::~VulkanImageDrawable() { release(); }

// ── Lifecycle ───────────────────────────────────────────────────────

void VulkanImageDrawable::init(Film &film) {
    if (_initialized) return;

    const int frame_count = film.concurrent_frame_count();

    _transform_ubos.resize(frame_count);
    _params_ubos.resize(frame_count);
    _descriptor_sets.resize(frame_count);

    for (int i = 0; i < frame_count; ++i) {
        // Create host-visible UBO buffers.
        _transform_ubos[i] =
            VulkanBuffer(film,
                         sizeof(ImageTransformUBO),
                         vk::BufferUsageFlagBits::eUniformBuffer,
                         vk::MemoryPropertyFlagBits::eHostVisible
                             | vk::MemoryPropertyFlagBits::eHostCoherent);

        _params_ubos[i] =
            VulkanBuffer(film,
                         sizeof(ImageParamsUBO),
                         vk::BufferUsageFlagBits::eUniformBuffer,
                         vk::MemoryPropertyFlagBits::eHostVisible
                             | vk::MemoryPropertyFlagBits::eHostCoherent);

        // Allocate descriptor set.  We cannot write it yet because the
        // texture may not exist.  We will write/update descriptors in
        // sync_from_image_data() once the texture is created.
        _descriptor_sets[i] = _manager->allocate_descriptor_set();
    }

    _initialized = true;
}

void VulkanImageDrawable::release() {
    _texture.release();

    for (auto &ubo : _transform_ubos) ubo.release();
    _transform_ubos.clear();

    for (auto &ubo : _params_ubos) ubo.release();
    _params_ubos.clear();

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

void VulkanImageDrawable::draw(const scene_graph::Camera &cam, Film &film) {
    if (!_initialized) return;
    if (!object().visible) return;

    // Sync GPU texture from ImageData if dirty.
    sync_from_image_data(film);

    if (!_texture.is_valid()) return;

    // Upload UBOs for this frame.
    update_ubos(cam, film);

    // Record draw commands.
    record_draw_commands(film);
}

// ── Private: sync ───────────────────────────────────────────────────

void VulkanImageDrawable::sync_from_image_data(Film &film) {
    auto *image_data = object().find_feature<scene_graph::ImageData>();
    if (!image_data) return;
    if (!image_data->has_pixels()) return;
    if (image_data->version() == _synced_version) return;

    uint32_t w = image_data->width();
    uint32_t h = image_data->height();
    auto format = static_cast<VulkanTexture::Format>(image_data->format());

    // Check if we need to (re)create the texture (dimensions or format
    // changed).
    bool need_recreate = !_texture.is_valid() || _texture.width() != w
                         || _texture.height() != h
                         || _texture.format() != format;

    if (need_recreate) {
        _texture.create(film, w, h, format);

        // Upload the full image.
        auto pixels = image_data->pixels();
        _texture.upload(film, pixels.data(), pixels.size());

        // Write descriptor sets now that texture exists.
        for (int i = 0; i < static_cast<int>(_descriptor_sets.size()); ++i) {
            _manager->write_descriptor_set(_descriptor_sets[i],
                                           _transform_ubos[i].buffer(),
                                           sizeof(ImageTransformUBO),
                                           _params_ubos[i].buffer(),
                                           sizeof(ImageParamsUBO),
                                           _texture.image_view(),
                                           _texture.sampler());
        }
    } else {
        // Texture exists and dimensions match — check for partial vs full
        // update.
        auto dirty = image_data->dirty_region();
        if (dirty && !image_data->is_full_dirty()) {
            // Partial update: extract the dirty sub-region from the pixel
            // buffer and upload just that rectangle.
            size_t bpp = image_data->bytes_per_pixel();
            size_t region_bytes =
                static_cast<size_t>(dirty->w) * dirty->h * bpp;
            size_t row_bytes = static_cast<size_t>(dirty->w) * bpp;

            // Build a tightly-packed buffer for the dirty region.
            std::vector<std::byte> region_data(region_bytes);
            auto pixels = image_data->pixels();
            for (uint32_t row = 0; row < dirty->h; ++row) {
                size_t src_offset =
                    (static_cast<size_t>(dirty->y + row) * w + dirty->x) * bpp;
                size_t dst_offset = static_cast<size_t>(row) * row_bytes;
                std::memcpy(region_data.data() + dst_offset,
                            pixels.data() + src_offset,
                            row_bytes);
            }

            _texture.update_region(film,
                                   dirty->x,
                                   dirty->y,
                                   dirty->w,
                                   dirty->h,
                                   region_data.data(),
                                   region_data.size());
        } else {
            // Full re-upload.
            auto pixels = image_data->pixels();
            _texture.upload(film, pixels.data(), pixels.size());
        }
    }

    image_data->clear_dirty();
    _synced_version = image_data->version();
}

// ── Private: UBO update ─────────────────────────────────────────────

void VulkanImageDrawable::update_ubos(const scene_graph::Camera &cam,
                                      Film &film) {
    if (!_initialized) return;

    const int fi = film.current_frame();

    // ── TransformUBO (MVP) ──────────────────────────────────────────
    ImageTransformUBO transform;
    if (_has_mvp_override) {
        transform.mvp = _mvp_override;
    } else {
        // Compute MVP from camera + object world transform.
        auto model = object().world_transform().to_matrix();
        auto view = cam.view_matrix();
        auto projection = cam.projection_matrix();
        transform.mvp = (projection * view * model).eval();
    }
    _transform_ubos[fi].upload(&transform, sizeof(ImageTransformUBO));

    // ── ImageParamsUBO (tone mapping + image dimensions) ────────────
    auto *image_data = object().find_feature<scene_graph::ImageData>();

    ImageParamsUBO params;
    float exposure = image_data ? image_data->exposure() : 0.0f;
    float gamma = image_data ? image_data->gamma() : 2.2f;
    float channel =
        image_data ? static_cast<float>(image_data->channel_mode()) : 0.0f;
    float img_w = image_data ? static_cast<float>(image_data->width()) : 1.0f;
    float img_h = image_data ? static_cast<float>(image_data->height()) : 1.0f;

    params.tone_params(0) = exposure;
    params.tone_params(1) = gamma;
    params.tone_params(2) = channel;
    params.tone_params(3) = 0.0f;

    params.image_size(0) = img_w;
    params.image_size(1) = img_h;
    params.image_size(2) = (img_w > 0.0f) ? 1.0f / img_w : 0.0f;
    params.image_size(3) = (img_h > 0.0f) ? 1.0f / img_h : 0.0f;

    _params_ubos[fi].upload(&params, sizeof(ImageParamsUBO));
}

// ── Private: draw commands ──────────────────────────────────────────

void VulkanImageDrawable::record_draw_commands(Film &film) {
    auto cb = film.current_command_buffer();
    const int fi = film.current_frame();

    auto pipeline = _manager->get_or_create(film);
    if (!pipeline) return;

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

    cb.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline);
    cb.setViewport(0, {viewport});
    cb.setScissor(0, {scissor});

    cb.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
                          _manager->pipeline_layout(),
                          0,
                          {_descriptor_sets[fi]},
                          {});

    // Fullscreen triangle: 3 vertices, no vertex buffer.
    cb.draw(3, 1, 0, 0);
}

} // namespace balsa::visualization::vulkan
