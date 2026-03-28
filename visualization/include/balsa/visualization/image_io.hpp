#if !defined(BALSA_VISUALIZATION_IMAGE_IO_HPP)
#define BALSA_VISUALIZATION_IMAGE_IO_HPP

#include <cstdint>
#include <expected>
#include <string>
#include <string_view>
#include <vector>

namespace balsa::visualization {

// ── PPM image I/O ──────────────────────────────────────────────────
//
// Minimal PPM (Portable Pixmap) reader and writer.
// Supports P6 (binary RGB, 8-bit) format only.
// No external dependencies.

struct ImageBuffer {
    uint32_t width = 0;
    uint32_t height = 0;
    // RGBA8 pixel data (4 bytes per pixel, row-major, top-to-bottom).
    std::vector<uint8_t> pixels;
};

enum class ImageIOError {
    FileNotFound,
    InvalidFormat,
    ReadError,
    WriteError,
};

auto error_string(ImageIOError err) -> std::string_view;

// Load a PPM (P6) image file.  Returns RGBA8 pixel data (the PPM RGB
// is expanded to RGBA with alpha = 255).
auto load_ppm(const std::string &path)
    -> std::expected<ImageBuffer, ImageIOError>;

// Save an RGBA8 image as a PPM (P6) file.  Alpha channel is discarded.
auto save_ppm(const std::string &path,
              uint32_t width,
              uint32_t height,
              const uint8_t *rgba_pixels) -> std::expected<void, ImageIOError>;

} // namespace balsa::visualization

#endif
