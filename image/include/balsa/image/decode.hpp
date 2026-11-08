#pragma once
#include "Image.hpp"

#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
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
    // The result's channels, converted if stored differently; nullopt keeps
    // the stored ones.
    std::optional<Channels> channels = std::nullopt;
};

// Decodes an encoded image (PNG, JPEG, BMP, TGA, GIF's first frame, PSD,
// HDR, PNM), detected from its contents.  Keeps the stored precision:
// 8-bit, 16-bit (PNG, PNM) or float (HDR).
[[nodiscard]] auto decode(std::span<const std::byte> bytes,
                          const DecodeOptions &options = {})
    -> std::expected<Image, DecodeError>;

} // namespace balsa::image
