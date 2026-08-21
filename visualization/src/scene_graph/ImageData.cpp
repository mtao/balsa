#include "balsa/scene_graph/ImageData.hpp"

#include <algorithm>
#include <cstring>
#include <limits>
#include <stdexcept>

namespace balsa::scene_graph {

namespace {
auto checked_image_size(uint32_t width, uint32_t height, size_t bpp) -> size_t {
    constexpr auto max = std::numeric_limits<size_t>::max();
    if (height != 0 && width > max / height) {
        throw std::runtime_error("ImageData: image dimensions overflow");
    }
    const size_t pixels = static_cast<size_t>(width) * height;
    if (bpp != 0 && pixels > max / bpp) {
        throw std::runtime_error("ImageData: image byte size overflow");
    }
    return pixels * bpp;
}
} // namespace

// ── Helpers ─────────────────────────────────────────────────────────

auto ImageData::bytes_per_pixel() const -> size_t {
    switch (_format) {
    case Format::RGBA8:
        return 4;
    case Format::RGBAF32:
        return 16;
    }
    return 4;
}

// ── set_pixels (full image replacement) ─────────────────────────────

auto ImageData::set_pixels(uint32_t width,
                           uint32_t height,
                           Format format,
                           std::span<const std::byte> data) -> void {
    if (width == 0 || height == 0) {
        throw std::runtime_error("ImageData::set_pixels: dimensions must be nonzero");
    }
    size_t bpp = (format == Format::RGBAF32) ? 16 : 4;
    size_t expected = checked_image_size(width, height, bpp);
    if (data.size() < expected) {
        throw std::runtime_error("ImageData::set_pixels: insufficient data");
    }

    _width = width;
    _height = height;
    _format = format;
    _pixels.assign(data.begin(), data.begin() + expected);
    ++_version;
    _dirty = DirtyRegion(0, 0, width, height);
    _full_dirty = true;
}

auto ImageData::set_pixels_rgba8(uint32_t width,
                                 uint32_t height,
                                 std::span<const uint8_t> rgba) -> void {
    auto bytes = std::as_bytes(rgba);
    set_pixels(width, height, Format::RGBA8, bytes);
}

auto ImageData::set_pixels_rgbaf32(uint32_t width,
                                   uint32_t height,
                                   std::span<const float> rgba) -> void {
    auto bytes = std::as_bytes(rgba);
    set_pixels(width, height, Format::RGBAF32, bytes);
}

// ── update_region (partial update) ──────────────────────────────────

auto ImageData::update_region(uint32_t x,
                              uint32_t y,
                              uint32_t w,
                              uint32_t h,
                              std::span<const std::byte> data) -> void {
    if (_pixels.empty()) {
        throw std::runtime_error("ImageData::update_region: no image set");
    }
    if (w == 0 || h == 0) {
        throw std::runtime_error(
            "ImageData::update_region: dimensions must be nonzero");
    }

    if (x > _width || y > _height || w > _width - x || h > _height - y) {
        throw std::runtime_error(
            "ImageData::update_region: region exceeds image bounds");
    }

    size_t bpp = bytes_per_pixel();
    size_t expected = checked_image_size(w, h, bpp);
    if (data.size() < expected) {
        throw std::runtime_error("ImageData::update_region: insufficient data");
    }

    // Copy rows into the pixel buffer.
    size_t row_bytes = static_cast<size_t>(w) * bpp;
    for (uint32_t row = 0; row < h; ++row) {
        size_t src_offset = static_cast<size_t>(row) * row_bytes;
        size_t dst_offset = (static_cast<size_t>(y + row) * _width + x) * bpp;
        std::memcpy(
            _pixels.data() + dst_offset, data.data() + src_offset, row_bytes);
    }

    ++_version;
    _full_dirty = false;

    // Merge with existing dirty region (union of rectangles).
    if (_dirty) {
        uint32_t x0 = std::min(_dirty->min(0), x);
        uint32_t y0 = std::min(_dirty->min(1), y);
        uint32_t x1 = std::max(_dirty->min(0) + _dirty->width(), x + w);
        uint32_t y1 = std::max(_dirty->min(1) + _dirty->height(), y + h);
        _dirty = DirtyRegion(x0, y0, x1, y1);
    } else {
        _dirty = DirtyRegion(x, y, x + w, y + h);
    }
}

} // namespace balsa::scene_graph
