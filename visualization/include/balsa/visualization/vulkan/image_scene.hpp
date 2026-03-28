#if !defined(BALSA_VISUALIZATION_VULKAN_IMAGE_SCENE_HPP)
#define BALSA_VISUALIZATION_VULKAN_IMAGE_SCENE_HPP

#include <cstdint>
#include <memory>
#include <span>
#include <string>

#include "balsa/scene_graph/Camera.hpp"
#include "balsa/scene_graph/DrawableGroup.hpp"
#include "balsa/scene_graph/ImageData.hpp"
#include "balsa/scene_graph/Object.hpp"
#include "balsa/visualization/vulkan/image_pipeline.hpp"
#include "balsa/visualization/vulkan/scene_base.hpp"

namespace balsa::visualization::vulkan {

class VulkanImageDrawable;

// ── ImageScene ──────────────────────────────────────────────────────
//
// SceneBase subclass for 2D image viewing with orthographic projection,
// pan/zoom navigation, and tone-mapping display parameters.
//
// Owns:
//   - A root scene_graph::Object (the scene graph root)
//   - A camera Object (child of root) with Camera feature
//   - A DrawableGroup (flat registry of VulkanImageDrawables)
//   - An ImagePipelineManager (shared by all drawables)
//
// The scene manages a single image Object internally.  Pan and zoom
// manipulate the orthographic projection / MVP override on the
// drawable, keeping the image's texture coordinates unchanged.
//
// Lifecycle:
//   1. Construct ImageScene
//   2. Set image data via set_image() or image_data()
//   3. Call initialize(film) — creates pipeline manager, inits drawables
//   4. Each frame: Window calls begin_render_pass -> draw -> end_render_pass
//   5. On teardown: release_vulkan_resources()
//
// Thread-safety: NOT thread-safe — call only from the rendering thread.

class ImageScene : public SceneBase {
  public:
    ImageScene();
    ~ImageScene() override;

    // Non-copyable, non-movable
    ImageScene(const ImageScene &) = delete;
    ImageScene &operator=(const ImageScene &) = delete;
    ImageScene(ImageScene &&) = delete;
    ImageScene &operator=(ImageScene &&) = delete;

    // ── SceneBase overrides ─────────────────────────────────────────

    void initialize(Film &film) override;
    void draw(Film &film) override;
    void release_vulkan_resources() override;

    // ── Image management ────────────────────────────────────────────

    // Set the full image data.  Creates the internal ImageData +
    // Object + VulkanImageDrawable on first call.
    void set_image(uint32_t width,
                   uint32_t height,
                   scene_graph::ImageData::Format format,
                   std::span<const std::byte> pixels);

    // Convenience overloads.
    void set_image_rgba8(uint32_t width,
                         uint32_t height,
                         std::span<const uint8_t> rgba);
    void set_image_rgbaf32(uint32_t width,
                           uint32_t height,
                           std::span<const float> rgba);

    // Access the underlying ImageData for direct manipulation
    // (e.g., partial updates from a live render, or changing
    // tone-mapping parameters).
    // Returns nullptr if no image has been set yet.
    scene_graph::ImageData *image_data();
    const scene_graph::ImageData *image_data() const;

    bool has_image() const;

    // ── 2D navigation ───────────────────────────────────────────────

    // Zoom level: 1.0 = 1 image pixel = 1 screen pixel (at fit).
    // Values > 1 zoom in, < 1 zoom out.
    void set_zoom(float zoom);
    float zoom() const { return _zoom; }

    // Pan offset in NDC units (-1 to 1).
    void set_pan(float x, float y);
    float pan_x() const { return _pan_x; }
    float pan_y() const { return _pan_y; }

    // Reset zoom and pan to fit the image in the viewport.
    void fit_to_window();

    // ── Camera ──────────────────────────────────────────────────────

    scene_graph::Camera &camera();
    const scene_graph::Camera &camera() const;

    // ── Scene graph access ──────────────────────────────────────────

    scene_graph::Object &root() { return _scene_root; }
    const scene_graph::Object &root() const { return _scene_root; }

    scene_graph::DrawableGroup &drawable_group() { return _drawable_group; }
    const scene_graph::DrawableGroup &drawable_group() const {
        return _drawable_group;
    }

    ImagePipelineManager &pipeline_manager() { return _pipeline_manager; }
    const ImagePipelineManager &pipeline_manager() const {
        return _pipeline_manager;
    }

  private:
    // Ensure the image Object and its features exist.
    void ensure_image_object();

    // Recompute the MVP matrix from zoom/pan state and update the
    // VulkanImageDrawable's override.
    void update_mvp();

    // Scene graph
    scene_graph::Object _scene_root;
    scene_graph::Object *_camera_object = nullptr;
    scene_graph::Camera *_camera = nullptr;
    scene_graph::Object *_image_object = nullptr;
    scene_graph::DrawableGroup _drawable_group;

    // Vulkan resources
    ImagePipelineManager _pipeline_manager;
    Film *_film = nullptr;
    bool _initialized = false;

    // 2D navigation state
    float _zoom = 1.0f;
    float _pan_x = 0.0f;
    float _pan_y = 0.0f;
};

} // namespace balsa::visualization::vulkan

#endif
