#include <balsa/image/decode.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#define STB_IMAGE_WRITE_STATIC
#define STB_IMAGE_WRITE_IMPLEMENTATION
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#pragma GCC diagnostic ignored "-Wsign-compare"
#pragma GCC diagnostic ignored "-Wunused-function"
#endif
#include <stb_image_write.h>
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

namespace image = balsa::image;
using Catch::Matchers::ContainsSubstring;

namespace {
// Collects stb_image_write's output.
void append(void *context, void *data, int size) {
    auto &out = *static_cast<std::vector<std::byte> *>(context);
    const auto *bytes = static_cast<const std::byte *>(data);
    out.insert(out.end(), bytes, bytes + size);
}

// A 3 x 2 RGBA image: pixel (row, column) has red 10 * column, green
// 100 * row, blue 7 and alpha 255 - column.
auto rgba_pixels() -> std::vector<uint8_t> {
    std::vector<uint8_t> pixels;
    for (uint8_t row = 0; row < 2; ++row) {
        for (uint8_t column = 0; column < 3; ++column) {
            pixels.insert(pixels.end(), {static_cast<uint8_t>(10 * column),
                                         static_cast<uint8_t>(100 * row), 7,
                                         static_cast<uint8_t>(255 - column)});
        }
    }
    return pixels;
}
} // namespace

TEST_CASE("image_decode_png", "[image]") {
    const auto pixels = rgba_pixels();
    std::vector<std::byte> png;
    REQUIRE(stbi_write_png_to_func(append, &png, 3, 2, 4, pixels.data(), 3 * 4));

    const auto decoded = image::decode(png);
    REQUIRE(decoded);
    CHECK(decoded->width() == 3);
    CHECK(decoded->height() == 2);
    CHECK(decoded->channels() == 4);
    CHECK(decoded->component() == image::ComponentType::UInt8);
    CHECK_FALSE(decoded->view<float>());
    const auto view = *decoded->view<uint8_t>();
    CHECK(view[1, 2, 0] == 20);
    CHECK(view[1, 2, 1] == 100);
    CHECK(view[1, 2, 2] == 7);
    CHECK(view[1, 2, 3] == 253);
    CHECK(std::ranges::equal(std::span(view.data_handle(), view.size()), pixels));

    SECTION("fixed formats") {
        CHECK_FALSE(decoded->view<uint8_t, 3>());
        const auto rgba = decoded->view<uint8_t, 4>();
        REQUIRE(rgba);
        STATIC_CHECK(decltype(rgba)::value_type::static_extent(2) == 4);
        CHECK((*rgba)[1, 2, 0] == 20);

        CHECK_FALSE(decoded->array<uint16_t, 4>());
        const auto array = decoded->array<uint8_t, 4>();
        REQUIRE(array);
        CHECK((*array)(1, 2, 1) == 100);
        // Coefficient-wise operations.
        CHECK((*array == uint8_t{7}).any());
        CHECK_FALSE((*array == uint8_t{42}).any());
    }

    SECTION("converting channels") {
        const auto rgb = image::decode(png, {.channels = 3});
        REQUIRE(rgb);
        CHECK(rgb->channels() == 3);
        CHECK((*rgb->view<uint8_t>())[1, 2, 0] == 20);

        CHECK_FALSE(image::decode(png, {.channels = 5}));
    }
}

TEST_CASE("image_decode_hdr", "[image]") {
    const std::array<float, 6> pixels{0.5f, 2.0f, 8.0f, 1.0f, 0.25f, 4.0f};
    std::vector<std::byte> hdr;
    REQUIRE(stbi_write_hdr_to_func(append, &hdr, 2, 1, 3, pixels.data()));

    const auto decoded = image::decode(hdr);
    REQUIRE(decoded);
    CHECK(decoded->component() == image::ComponentType::Float32);
    const auto view = *decoded->view<float>();
    // Radiance HDR shares an exponent per pixel; these values are exact.
    CHECK(view[0, 0, 1] == 2.0f);
    CHECK(view[0, 1, 2] == 4.0f);
}

TEST_CASE("image_decode_jpeg", "[image]") {
    const std::vector<uint8_t> grey(8 * 8, 128);
    std::vector<std::byte> jpeg;
    REQUIRE(stbi_write_jpg_to_func(append, &jpeg, 8, 8, 1, grey.data(), 95));

    // stb_image_write stores colour even for grey input.
    const auto decoded = image::decode(jpeg, {.channels = 1});
    REQUIRE(decoded);
    CHECK(decoded->channels() == 1);
    const auto value = (*decoded->view<uint8_t>())[4, 4, 0];
    CHECK(value >= 126);
    CHECK(value <= 130);
}

TEST_CASE("image_decode_errors", "[image]") {
    const std::array<std::byte, 4> junk{std::byte{1}, std::byte{2}, std::byte{3}, std::byte{4}};
    const auto unknown = image::decode(junk);
    REQUIRE_FALSE(unknown);
    CHECK(unknown.error().code == image::DecodeError::Code::UnsupportedFormat);

    // A valid header with its pixel data cut off.
    const auto pixels = rgba_pixels();
    std::vector<std::byte> png;
    REQUIRE(stbi_write_png_to_func(append, &png, 3, 2, 4, pixels.data(), 3 * 4));
    png.resize(png.size() - 20);
    const auto truncated = image::decode(png);
    REQUIRE_FALSE(truncated);
    CHECK(truncated.error().code == image::DecodeError::Code::Corrupt);
    CHECK_THAT(truncated.error().message, ContainsSubstring("corrupt"));
}
