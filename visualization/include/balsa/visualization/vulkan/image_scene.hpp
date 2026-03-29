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
    auto operator=(const ImageScene &) -> ImageScene & = delete;
    ImageScene(ImageScene &&) = delete;
    auto operator=(ImageScene &&) -> ImageScene & = delete;

    // ── SceneBase overrides ─────────────────────────────────────────

    auto initialize(Film &film) -> void override;
    auto draw(Film &film) -> void override;
    auto release_vulkan_resources() -> void override;

    // ── Image management ────────────────────────────────────────────

    // Set the full image data.  Creates the internal ImageData +
    // Object + VulkanImageDrawable on first call.
    auto set_image(uint32_t width,
                   uint32_t height,
                   scene_graph::ImageData::Format format,
                   std::span<const std::byte> pixels) -> void;

    // Convenience overloads.
    auto set_image_rgba8(uint32_t width,
                         uint32_t height,
                         std::span<const uint8_t> rgba) -> void;
    auto set_image_rgbaf32(uint32_t width,
                           uint32_t height,
                           std::span<const float> rgba) -> void;

    // Access the underlying ImageData for direct manipulation
    // (e.g., partial updates from a live render, or changing
    // tone-mapping parameters).
    // Returns nullptr if no image has been set yet.
    auto image_data() -> scene_graph::ImageData *;
    auto image_data() const -> const scene_graph::ImageData *;

    auto has_image() const -> bool;

    // ── 2D navigation ───────────────────────────────────────────────

    // Zoom level: 1.0 = 1 image pixel = 1 screen pixel (at fit).
    // Values > 1 zoom in, < 1 zoom out.
    auto set_zoom(float zoom) -> void;
    auto zoom() const -> float { return _zoom; }

    // Pan offset in NDC units (-1 to 1).
    auto set_pan(float x, float y) -> void;
    auto pan_x() const -> float { return _pan_x; }
    auto pan_y() const -> float { return _pan_y; }

    // Reset zoom and pan to fit the image in the viewport.
    auto fit_to_window() -> void;

    // ── Camera ──────────────────────────────────────────────────────

    auto camera() -> scene_graph::Camera &;
    auto camera() const -> const scene_graph::Camera &;

    // ── Scene graph access ──────────────────────────────────────────

    auto root() -> scene_graph::Object & { return _scene_root; }
    auto root() const -> const scene_graph::Object & { return _scene_root; }

    auto drawable_group() -> scene_graph::DrawableGroup & { return _drawable_group; }
    auto drawable_group() const -> const scene_graph::DrawableGroup & {
        return _drawable_group;
    }

    auto pipeline_manager() -> ImagePipelineManager & { return _pipeline_manager; }
    auto pipeline_manager() const -> const ImagePipelineManager & {
        return _pipeline_manager;
    }

  private:
    // Ensure the image Object and its features exist.
    auto ensure_image_object() -> void;

    // Recompute the MVP matrix from zoom/pan state and update the
    // VulkanImageDrawable's override.
    auto update_mvp() -> void;

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
