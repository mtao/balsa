#if !defined(BALSA_SCENE_GRAPH_IMAGE_DATA_HPP)
#define BALSA_SCENE_GRAPH_IMAGE_DATA_HPP

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include "balsa/scene_graph/AbstractFeature.hpp"
#include "balsa/visualization/vulkan/texture.hpp"

namespace balsa::scene_graph {

// ── ImageData ───────────────────────────────────────────────────────
//
// Scene graph feature that holds CPU-side image pixel data.  Analogous
// to MeshData but for 2D images.  Owns the pixel buffer and tracks
// modifications via a version counter + dirty region so that the
// corresponding VulkanImageDrawable can sync only what changed.
//
// Supports two pixel formats:
//   - RGBA8:   4 bytes/pixel  (8-bit unsigned, sRGB)
//   - RGBAF32: 16 bytes/pixel (32-bit float, linear HDR)
//
// The tone-mapping display parameters (exposure, gamma, channel mode)
// are stored here but evaluated in the fragment shader on the GPU.

class ImageData : public AbstractFeature {
  public:
    using Format = visualization::vulkan::VulkanTexture::Format;

    ImageData() = default;

    // ── Pixel mutators ──────────────────────────────────────────────
    // Each bumps the version counter.

    // Set the full image.  Copies the data.
    auto set_pixels(uint32_t width,
                    uint32_t height,
                    Format format,
                    std::span<const std::byte> data) -> void;

    // Convenience overloads.
    auto set_pixels_rgba8(uint32_t width,
                          uint32_t height,
                          std::span<const uint8_t> rgba) -> void;
    auto set_pixels_rgbaf32(uint32_t width,
                            uint32_t height,
                            std::span<const float> rgba) -> void;

    // Partial update — marks a dirty region.
    // The data must be tightly packed (w * h * bytes_per_pixel).
    auto update_region(uint32_t x,
                       uint32_t y,
                       uint32_t w,
                       uint32_t h,
                       std::span<const std::byte> data) -> void;

    // ── Accessors ───────────────────────────────────────────────────

    auto width() const -> uint32_t { return _width; }
    auto height() const -> uint32_t { return _height; }
    auto format() const -> Format { return _format; }
    auto pixels() const -> std::span<const std::byte> { return _pixels; }
    auto has_pixels() const -> bool { return !_pixels.empty(); }

    // Bytes per pixel for the current format.
    auto bytes_per_pixel() const -> size_t;

    // ── Dirty tracking ──────────────────────────────────────────────

    auto version() const -> uint64_t { return _version; }

    // Dirty region: the rectangle that changed since last clear.
    // If no partial update has been done (or set_pixels was called),
    // this covers the full image.
    struct DirtyRegion {
        uint32_t x, y, w, h;
    };
    auto dirty_region() const -> std::optional<DirtyRegion> { return _dirty; }
    auto clear_dirty() -> void { _dirty = std::nullopt; }

    // Was the full image replaced since last clear?
    // (As opposed to just a partial region update.)
    auto is_full_dirty() const -> bool { return _full_dirty; }

    // ── Display parameters ──────────────────────────────────────────
    // Tone mapping (for HDR float images).

    auto exposure() const -> float { return _exposure; }
    auto set_exposure(float ev) -> void { _exposure = ev; }

    auto gamma() const -> float { return _gamma; }
    auto set_gamma(float g) -> void { _gamma = g; }

    // Channel display mode.
    enum class ChannelMode : int {
        RGBA = 0,
        Red = 1,
        Green = 2,
        Blue = 3,
        Alpha = 4,
        Luminance = 5,
    };
    auto channel_mode() const -> ChannelMode { return _channel_mode; }
    auto set_channel_mode(ChannelMode mode) -> void { _channel_mode = mode; }

  private:
    std::vector<std::byte> _pixels;
    uint32_t _width = 0, _height = 0;
    Format _format = Format::RGBA8;
    uint64_t _version = 0;
    std::optional<DirtyRegion> _dirty;
    bool _full_dirty = false;

    float _exposure = 0.0f;
    float _gamma = 2.2f;
    ChannelMode _channel_mode = ChannelMode::RGBA;
};

} // namespace balsa::scene_graph

#endif
