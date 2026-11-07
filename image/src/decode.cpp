#include "balsa/image/decode.hpp"

#include <algorithm>
#include <climits>
#include <format>
#include <memory>

// Internal linkage, so stb's symbols do not leak from the library.
#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_STDIO
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#pragma GCC diagnostic ignored "-Wsign-compare"
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"
#pragma GCC diagnostic ignored "-Wconversion"
#endif
#include <stb_image.h>
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

namespace balsa::image {

namespace {
    struct StbFree {
        void operator()(void *pixels) const { stbi_image_free(pixels); }
    };

    auto failure(DecodeError::Code code, std::string_view what) -> std::unexpected<DecodeError> {
        const char *reason = stbi_failure_reason();
        return std::unexpected(DecodeError{
            code, reason ? std::format("{}: {}", what, reason) : std::string(what)});
    }
} // namespace

auto decode(std::span<const std::byte> bytes, const DecodeOptions &options)
    -> std::expected<Image, DecodeError> {
    if (options.channels > 4) {
        return std::unexpected(DecodeError{DecodeError::Code::UnsupportedFormat,
                                           std::format("cannot decode to {} channels",
                                                       options.channels)});
    }
    if (bytes.size() > static_cast<std::size_t>(INT_MAX)) {
        return std::unexpected(
            DecodeError{DecodeError::Code::UnsupportedFormat, "encoded image exceeds 2 GiB"});
    }
    const auto *data = reinterpret_cast<const stbi_uc *>(bytes.data());
    const auto length = static_cast<int>(bytes.size());

    int width = 0, height = 0, stored = 0;
    if (!stbi_info_from_memory(data, length, &width, &height, &stored)) {
        return failure(DecodeError::Code::UnsupportedFormat, "unrecognised image format");
    }
    const auto requested = static_cast<int>(options.channels);
    const auto component = stbi_is_hdr_from_memory(data, length)  ? ComponentType::Float32
                           : stbi_is_16_bit_from_memory(data, length) ? ComponentType::UInt16
                                                                     : ComponentType::UInt8;

    std::unique_ptr<void, StbFree> pixels;
    switch (component) {
    case ComponentType::Float32:
        pixels.reset(stbi_loadf_from_memory(data, length, &width, &height, &stored, requested));
        break;
    case ComponentType::UInt16:
        pixels.reset(stbi_load_16_from_memory(data, length, &width, &height, &stored, requested));
        break;
    case ComponentType::UInt8:
        pixels.reset(stbi_load_from_memory(data, length, &width, &height, &stored, requested));
        break;
    }
    if (!pixels) return failure(DecodeError::Code::Corrupt, "corrupt image");

    Image image(Image::Extents(height, width, requested ? requested : stored), component);
    const auto *source = static_cast<const std::byte *>(pixels.get());
    std::ranges::copy(std::span(source, image.bytes().size()), image.bytes().begin());
    return image;
}

} // namespace balsa::image
