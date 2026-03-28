#include "balsa/visualization/image_io.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>

namespace balsa::visualization {

auto error_string(ImageIOError err) -> std::string_view {
    switch (err) {
    case ImageIOError::FileNotFound:
        return "File not found";
    case ImageIOError::InvalidFormat:
        return "Invalid image format";
    case ImageIOError::ReadError:
        return "Read error";
    case ImageIOError::WriteError:
        return "Write error";
    }
    return "Unknown error";
}

// ── PPM P6 reader ──────────────────────────────────────────────────
//
// P6 binary format:
//   P6\n
//   <width> <height>\n
//   <maxval>\n
//   <width*height*3 bytes of RGB data>
//
// Comments (lines starting with '#') may appear between the header
// fields.

auto load_ppm(const std::string &path)
    -> std::expected<ImageBuffer, ImageIOError> {
    std::FILE *f = std::fopen(path.c_str(), "rb");
    if (!f) { return std::unexpected(ImageIOError::FileNotFound); }

    // Helper to skip comment lines.
    auto skip_comments = [&]() {
        int c;
        while ((c = std::fgetc(f)) == '#') {
            // Skip to end of line.
            while ((c = std::fgetc(f)) != '\n' && c != EOF) {}
        }
        if (c != EOF) std::ungetc(c, f);
    };

    // Read magic number.
    char magic[3] = {};
    if (std::fread(magic, 1, 2, f) != 2 || magic[0] != 'P' || magic[1] != '6') {
        std::fclose(f);
        return std::unexpected(ImageIOError::InvalidFormat);
    }

    // Skip whitespace + comments.
    skip_comments();

    // Read width and height.
    int width = 0, height = 0;
    if (std::fscanf(f, "%d %d", &width, &height) != 2 || width <= 0
        || height <= 0) {
        std::fclose(f);
        return std::unexpected(ImageIOError::InvalidFormat);
    }

    skip_comments();

    // Read maxval.
    int maxval = 0;
    if (std::fscanf(f, "%d", &maxval) != 1 || maxval <= 0 || maxval > 255) {
        std::fclose(f);
        return std::unexpected(ImageIOError::InvalidFormat);
    }

    // Single whitespace character after maxval.
    std::fgetc(f);

    // Read RGB data.
    size_t pixel_count = static_cast<size_t>(width) * height;
    std::vector<uint8_t> rgb(pixel_count * 3);
    if (std::fread(rgb.data(), 1, rgb.size(), f) != rgb.size()) {
        std::fclose(f);
        return std::unexpected(ImageIOError::ReadError);
    }

    std::fclose(f);

    // Convert RGB -> RGBA.
    ImageBuffer result;
    result.width = static_cast<uint32_t>(width);
    result.height = static_cast<uint32_t>(height);
    result.pixels.resize(pixel_count * 4);

    for (size_t i = 0; i < pixel_count; ++i) {
        result.pixels[i * 4 + 0] = rgb[i * 3 + 0];
        result.pixels[i * 4 + 1] = rgb[i * 3 + 1];
        result.pixels[i * 4 + 2] = rgb[i * 3 + 2];
        result.pixels[i * 4 + 3] = 255;
    }

    return result;
}

// ── PPM P6 writer ──────────────────────────────────────────────────

auto save_ppm(const std::string &path,
              uint32_t width,
              uint32_t height,
              const uint8_t *rgba_pixels) -> std::expected<void, ImageIOError> {
    std::FILE *f = std::fopen(path.c_str(), "wb");
    if (!f) { return std::unexpected(ImageIOError::WriteError); }

    // Write header.
    std::fprintf(f, "P6\n%u %u\n255\n", width, height);

    // Write RGB data (drop alpha).
    size_t pixel_count = static_cast<size_t>(width) * height;
    std::vector<uint8_t> rgb(pixel_count * 3);
    for (size_t i = 0; i < pixel_count; ++i) {
        rgb[i * 3 + 0] = rgba_pixels[i * 4 + 0];
        rgb[i * 3 + 1] = rgba_pixels[i * 4 + 1];
        rgb[i * 3 + 2] = rgba_pixels[i * 4 + 2];
    }

    if (std::fwrite(rgb.data(), 1, rgb.size(), f) != rgb.size()) {
        std::fclose(f);
        return std::unexpected(ImageIOError::WriteError);
    }

    std::fclose(f);
    return {};
}

} // namespace balsa::visualization
