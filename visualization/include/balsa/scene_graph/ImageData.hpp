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
    void set_pixels(uint32_t width,
                    uint32_t height,
                    Format format,
                    std::span<const std::byte> data);

    // Convenience overloads.
    void set_pixels_rgba8(uint32_t width,
                          uint32_t height,
                          std::span<const uint8_t> rgba);
    void set_pixels_rgbaf32(uint32_t width,
                            uint32_t height,
                            std::span<const float> rgba);

    // Partial update — marks a dirty region.
    // The data must be tightly packed (w * h * bytes_per_pixel).
    void update_region(uint32_t x,
                       uint32_t y,
                       uint32_t w,
                       uint32_t h,
                       std::span<const std::byte> data);

    // ── Accessors ───────────────────────────────────────────────────

    uint32_t width() const { return _width; }
    uint32_t height() const { return _height; }
    Format format() const { return _format; }
    std::span<const std::byte> pixels() const { return _pixels; }
    bool has_pixels() const { return !_pixels.empty(); }

    // Bytes per pixel for the current format.
    size_t bytes_per_pixel() const;

    // ── Dirty tracking ──────────────────────────────────────────────

    uint64_t version() const { return _version; }

    // Dirty region: the rectangle that changed since last clear.
    // If no partial update has been done (or set_pixels was called),
    // this covers the full image.
    struct DirtyRegion {
        uint32_t x, y, w, h;
    };
    std::optional<DirtyRegion> dirty_region() const { return _dirty; }
    void clear_dirty() { _dirty = std::nullopt; }

    // Was the full image replaced since last clear?
    // (As opposed to just a partial region update.)
    bool is_full_dirty() const { return _full_dirty; }

    // ── Display parameters ──────────────────────────────────────────
    // Tone mapping (for HDR float images).

    float exposure() const { return _exposure; }
    void set_exposure(float ev) { _exposure = ev; }

    float gamma() const { return _gamma; }
    void set_gamma(float g) { _gamma = g; }

    // Channel display mode.
    enum class ChannelMode : int {
        RGBA = 0,
        Red = 1,
        Green = 2,
        Blue = 3,
        Alpha = 4,
        Luminance = 5,
    };
    ChannelMode channel_mode() const { return _channel_mode; }
    void set_channel_mode(ChannelMode mode) { _channel_mode = mode; }

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
