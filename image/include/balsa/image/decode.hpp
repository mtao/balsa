#pragma once
#include "Image.hpp"

#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <string>

namespace balsa::image {

struct DecodeError {
    enum class Code {
        UnsupportedFormat,
        Corrupt,
    };
    Code code;
    std::string message;
};

struct DecodeOptions {
    // Channels in the result (1 to 4), converting if stored differently;
    // 0 keeps the stored count.
    uint32_t channels = 0;
};

// Decodes an encoded image (PNG, JPEG, BMP, TGA, GIF's first frame, PSD,
// HDR, PNM), detected from its contents.  Keeps the stored precision:
// 8-bit, 16-bit (PNG, PNM) or float (HDR).
[[nodiscard]] auto decode(std::span<const std::byte> bytes, const DecodeOptions &options = {})
    -> std::expected<Image, DecodeError>;

} // namespace balsa::image
