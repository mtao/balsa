#include "balsa/visualization/image_io.hpp"

#include <cstdio>
#include <charconv>
#include <cctype>
#include <fstream>
#include <limits>

namespace balsa::visualization {

namespace {
auto checked_pixel_count(uint32_t width, uint32_t height) -> size_t {
    constexpr auto max = std::numeric_limits<size_t>::max();
    if (height != 0 && width > max / height) {
        throw std::length_error("image dimensions overflow");
    }
    return static_cast<size_t>(width) * height;
}

auto read_ppm_token(std::FILE *file) -> std::string {
    std::string token;
    int c = 0;
    while ((c = std::fgetc(file)) != EOF) {
        if (std::isspace(static_cast<unsigned char>(c))) continue;
        if (c == '#') {
            while ((c = std::fgetc(file)) != '\n' && c != EOF) {}
            continue;
        }
        break;
    }
    while (c != EOF && !std::isspace(static_cast<unsigned char>(c))) {
        token.push_back(static_cast<char>(c));
        c = std::fgetc(file);
    }
    if (c == '\r') {
        const int next = std::fgetc(file);
        if (next != '\n' && next != EOF) std::ungetc(next, file);
    }
    return token;
}

auto parse_positive_u32(const std::string &token, uint32_t &value) -> bool {
    const auto *begin = token.data();
    const auto *end = begin + token.size();
    auto [ptr, ec] = std::from_chars(begin, end, value);
    return ec == std::errc{} && ptr == end && value > 0;
}
} // namespace

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

    if (read_ppm_token(f) != "P6") {
        std::fclose(f);
        return std::unexpected(ImageIOError::InvalidFormat);
    }

    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t maxval = 0;
    if (!parse_positive_u32(read_ppm_token(f), width)
        || !parse_positive_u32(read_ppm_token(f), height)
        || !parse_positive_u32(read_ppm_token(f), maxval) || maxval > 255) {
        std::fclose(f);
        return std::unexpected(ImageIOError::InvalidFormat);
    }

    size_t pixel_count = 0;
    try {
        pixel_count = checked_pixel_count(width, height);
        if (pixel_count > std::numeric_limits<size_t>::max() / 4) {
            throw std::length_error("image byte size overflow");
        }
    } catch (const std::length_error &) {
        std::fclose(f);
        return std::unexpected(ImageIOError::InvalidFormat);
    }

    // Read RGB data.
    std::vector<uint8_t> rgb(pixel_count * 3);
    if (std::fread(rgb.data(), 1, rgb.size(), f) != rgb.size()) {
        std::fclose(f);
        return std::unexpected(ImageIOError::ReadError);
    }

    std::fclose(f);

    // Convert RGB -> RGBA.
    ImageBuffer result;
    result.width = width;
    result.height = height;
    result.pixels.resize(pixel_count * 4);

    for (size_t i = 0; i < pixel_count; ++i) {
        for (size_t channel = 0; channel < 3; ++channel) {
            const auto sample = static_cast<uint32_t>(rgb[i * 3 + channel]);
            result.pixels[i * 4 + channel] =
                static_cast<uint8_t>((sample * 255 + maxval / 2) / maxval);
        }
        result.pixels[i * 4 + 3] = 255;
    }

    return result;
}

// ── PPM P6 writer ──────────────────────────────────────────────────

auto save_ppm(const std::string &path,
              uint32_t width,
              uint32_t height,
              std::span<const uint8_t> rgba_pixels)
    -> std::expected<void, ImageIOError> {
    if (width == 0 || height == 0) {
        return std::unexpected(ImageIOError::InvalidFormat);
    }
    size_t pixel_count = 0;
    try {
        pixel_count = checked_pixel_count(width, height);
        if (pixel_count > std::numeric_limits<size_t>::max() / 4
            || rgba_pixels.size() < pixel_count * 4) {
            return std::unexpected(ImageIOError::InvalidFormat);
        }
    } catch (const std::length_error &) {
        return std::unexpected(ImageIOError::InvalidFormat);
    }

    std::FILE *f = std::fopen(path.c_str(), "wb");
    if (!f) { return std::unexpected(ImageIOError::WriteError); }

    // Write header.
    std::fprintf(f, "P6\n%u %u\n255\n", width, height);

    // Write RGB data (drop alpha).
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
