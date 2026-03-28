#include "balsa/visualization/vulkan/image_scene.hpp"
#include "balsa/visualization/vulkan/film.hpp"
#include "balsa/visualization/vulkan/vulkan_image_drawable.hpp"

#include <algorithm>
#include <cstring>
#include <spdlog/spdlog.h>

namespace balsa::visualization::vulkan {

// ── Constructor / Destructor ────────────────────────────────────────

ImageScene::ImageScene() : _scene_root("Root") {
    // Create a camera Object as a child of the root.
    auto &cam_obj = _scene_root.add_child("Camera");
    cam_obj.permanent = true;
    _camera_object = &cam_obj;
    _camera = &cam_obj.emplace_feature<scene_graph::Camera>();

    // Start with a sensible orthographic projection.
    // Will be updated by fit_to_window() once an image is loaded.
    _camera->set_orthographic(-1.0f, 1.0f, -1.0f, 1.0f, -1.0f, 1.0f);
}

ImageScene::~ImageScene() { release_vulkan_resources(); }

// ── SceneBase overrides ─────────────────────────────────────────────

void ImageScene::initialize(Film &film) {
    SceneBase::initialize(film);
    _film = &film;

    const uint32_t max_sets =
        4 * static_cast<uint32_t>(film.concurrent_frame_count());
    _pipeline_manager.init(film, max_sets);
    _initialized = true;

    // Init any VulkanImageDrawables that were added before initialize().
    for (auto *drawable : _drawable_group) {
        auto *vid = dynamic_cast<VulkanImageDrawable *>(drawable);
        if (vid && !vid->is_initialized()) { vid->init(film); }
    }

    spdlog::info("ImageScene initialized with {} drawable(s)",
                 _drawable_group.size());
}

void ImageScene::draw(Film &film) {
    if (!_initialized) return;

    for (auto *drawable : _drawable_group) {
        auto *vid = dynamic_cast<VulkanImageDrawable *>(drawable);
        if (!vid) continue;
        vid->draw(*_camera, film);
    }
}

void ImageScene::release_vulkan_resources() {
    if (!_initialized) return;

    spdlog::debug("ImageScene: releasing Vulkan resources ({} drawables)",
                  _drawable_group.size());

    // Release all VulkanImageDrawables first (they reference pipeline
    // manager's descriptor pool).
    for (auto *drawable : _drawable_group) {
        auto *vid = dynamic_cast<VulkanImageDrawable *>(drawable);
        if (vid) vid->release();
    }

    // Then release the pipeline manager.
    _pipeline_manager.release();

    _film = nullptr;
    _initialized = false;
}

// ── Image management ────────────────────────────────────────────────

void ImageScene::ensure_image_object() {
    if (_image_object) return;

    auto &obj = _scene_root.add_child("Image");
    _image_object = &obj;
    obj.emplace_feature<scene_graph::ImageData>();
    auto &vid = obj.emplace_feature<VulkanImageDrawable>(_drawable_group,
                                                         _pipeline_manager);

    if (_initialized && _film) { vid.init(*_film); }
}

void ImageScene::set_image(uint32_t width,
                           uint32_t height,
                           scene_graph::ImageData::Format format,
                           std::span<const std::byte> pixels) {
    ensure_image_object();
    auto *img = _image_object->find_feature<scene_graph::ImageData>();
    img->set_pixels(width, height, format, pixels);
    update_mvp();
}

void ImageScene::set_image_rgba8(uint32_t width,
                                 uint32_t height,
                                 std::span<const uint8_t> rgba) {
    ensure_image_object();
    auto *img = _image_object->find_feature<scene_graph::ImageData>();
    img->set_pixels_rgba8(width, height, rgba);
    update_mvp();
}

void ImageScene::set_image_rgbaf32(uint32_t width,
                                   uint32_t height,
                                   std::span<const float> rgba) {
    ensure_image_object();
    auto *img = _image_object->find_feature<scene_graph::ImageData>();
    img->set_pixels_rgbaf32(width, height, rgba);
    update_mvp();
}

scene_graph::ImageData *ImageScene::image_data() {
    if (!_image_object) return nullptr;
    return _image_object->find_feature<scene_graph::ImageData>();
}

const scene_graph::ImageData *ImageScene::image_data() const {
    if (!_image_object) return nullptr;
    return _image_object->find_feature<scene_graph::ImageData>();
}

bool ImageScene::has_image() const {
    return _image_object != nullptr
           && _image_object->find_feature<scene_graph::ImageData>() != nullptr;
}

// ── 2D navigation ───────────────────────────────────────────────────

void ImageScene::set_zoom(float zoom) {
    _zoom = std::max(0.01f, zoom);
    update_mvp();
}

void ImageScene::set_pan(float x, float y) {
    _pan_x = x;
    _pan_y = y;
    update_mvp();
}

void ImageScene::fit_to_window() {
    _zoom = 1.0f;
    _pan_x = 0.0f;
    _pan_y = 0.0f;
    update_mvp();
}

// ── Camera accessors ────────────────────────────────────────────────

scene_graph::Camera &ImageScene::camera() { return *_camera; }
const scene_graph::Camera &ImageScene::camera() const { return *_camera; }

// ── Private: MVP computation ────────────────────────────────────────

void ImageScene::update_mvp() {
    if (!_image_object) return;

    auto *vid = _image_object->find_feature<VulkanImageDrawable>();
    if (!vid) return;

    // The fullscreen triangle shader maps gl_VertexIndex {0,1,2} to a
    // triangle covering the entire [-1,1] clip space and UV [0,1].
    // With an identity MVP, the image fills the viewport.
    //
    // To apply pan/zoom, we construct an MVP that:
    //   - Scales by _zoom (values > 1 enlarge the image)
    //   - Translates by (_pan_x, _pan_y) in NDC
    //
    // The vertex shader applies:
    //   gl_Position = mvp * vec4(clip_xy, 0.0, 1.0)
    //
    // So the MVP is a simple 2D scale + translate matrix.

    scene_graph::Mat4f mvp;
    // Start with identity
    mvp(0, 0) = _zoom;
    mvp(1, 1) = _zoom;
    mvp(2, 2) = 1.0f;
    mvp(3, 3) = 1.0f;

    // Off-diagonals zero
    mvp(0, 1) = 0.0f;
    mvp(0, 2) = 0.0f;
    mvp(0, 3) = 0.0f;
    mvp(1, 0) = 0.0f;
    mvp(1, 2) = 0.0f;
    mvp(1, 3) = 0.0f;
    mvp(2, 0) = 0.0f;
    mvp(2, 1) = 0.0f;
    mvp(2, 3) = 0.0f;
    mvp(3, 0) = 0.0f;
    mvp(3, 1) = 0.0f;
    mvp(3, 2) = 0.0f;

    // Translation: column 3 (column-major)
    mvp(0, 3) = _pan_x;
    mvp(1, 3) = _pan_y;

    vid->set_mvp_override(mvp);
}

} // namespace balsa::visualization::vulkan
