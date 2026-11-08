#include <balsa/image/convert.hpp>
#include <balsa/image/decode.hpp>

#include <zipper/TensorBase.hpp>
#include <zipper/as.hpp>

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
    CHECK(decoded->layout() == image::Channels::Rgba);
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
        // Coefficient-wise: the 7s are the six blue values.
        CHECK((*array == uint8_t{7}).select(1, 0).sum() == 6);
        CHECK_FALSE((*array == uint8_t{42}).any());
    }

    SECTION("converting channels") {
        const auto rgb = image::decode(png, {.channels = image::Channels::Rgb});
        REQUIRE(rgb);
        CHECK(rgb->channels() == 3);
        CHECK((*rgb->view<uint8_t>())[1, 2, 0] == 20);

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
    const auto decoded = image::decode(jpeg, {.channels = image::Channels::Grey});
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

TEST_CASE("image_convert", "[image]") {
    using enum image::ComponentType;
    // 2 x 1 grey + alpha, 8-bit: (51, 255), (255, 0).
    image::Image grey(1, 2, image::Channels::GreyAlpha, UInt8);
    auto g = *grey.view<uint8_t>();
    g[0, 0, 0] = 51;
    g[0, 0, 1] = 255;
    g[0, 1, 0] = 255;
    g[0, 1, 1] = 0;
    CHECK(grey.layout() == image::Channels::GreyAlpha);

    SECTION("channels: grey to RGBA keeps values and alpha") {
        const auto rgba = image::convert_channels(grey, image::Channels::Rgba);
        CHECK(rgba.component() == UInt8);
        const auto v = *rgba.view<uint8_t, 4>();
        CHECK(v[0, 0, 0] == 51);
        CHECK(v[0, 0, 2] == 51);
        CHECK(v[0, 0, 3] == 255);
        CHECK(v[0, 1, 3] == 0);
    }
    SECTION("channels: RGB to grey by luminance, opaque alpha") {
        image::Image rgb(1, 1, image::Channels::Rgb, UInt16);
        auto c = *rgb.view<uint16_t>();
        c[0, 0, 0] = 0;
        c[0, 0, 1] = 65535;
        c[0, 0, 2] = 0;
        const auto ga = image::convert_channels(rgb, image::Channels::GreyAlpha);
        const auto v = *ga.view<uint16_t, 2>();
        CHECK(v[0, 0, 0] == 46871); // 0.7152 * 65535
        CHECK(v[0, 0, 1] == 65535);
    }
    SECTION("components keep normalised values and channels") {
        const auto wide = image::convert_component(grey, UInt16);
        CHECK(wide.channels() == 2);
        CHECK((*wide.view<uint16_t>())[0, 0, 0] == 51 * 257);
        const auto real = image::convert_component(grey, Float32);
        CHECK((*real.view<float>())[0, 0, 0] == 0.2f);
        CHECK(image::convert_component(real, UInt8).bytes()[0] == std::byte{51});
    }
    SECTION("components: floats clamp to integer range") {
        image::Image hdr(1, 1, image::Channels::Rgb, Float32);
        auto h = *hdr.view<float>();
        h[0, 0, 0] = 2.0f;
        h[0, 0, 1] = -1.0f;
        h[0, 0, 2] = 0.5f;
        const auto ldr = image::convert_component(hdr, UInt8);
        const auto v = *ldr.view<uint8_t, 3>();
        CHECK(v[0, 0, 0] == 255);
        CHECK(v[0, 0, 1] == 0);
        CHECK(v[0, 0, 2] == 128);
    }
    SECTION("layouts are explicit") {
        CHECK(image::convert_component(grey, Float32).layout() == image::Channels::GreyAlpha);
        // Two channels of plain data (e.g. a normal map's XY) are not grey + alpha.
        image::Image data(image::Image::Extents(1, 1, 2), UInt8);
        CHECK_FALSE(data.layout());
        CHECK_THROWS_AS(image::convert_channels(data, image::Channels::Rgba), std::invalid_argument);
        CHECK_THROWS_AS(data.set_layout(image::Channels::Rgb), std::invalid_argument);
        data.set_layout(image::Channels::GreyAlpha);
        CHECK(image::convert_channels(data, image::Channels::Rgba).channels() == 4);
    }
    SECTION("unchanged formats copy") {
        CHECK(std::ranges::equal(image::convert_channels(grey, image::Channels::GreyAlpha).bytes(),
                                 grey.bytes()));
        CHECK(std::ranges::equal(image::convert_component(grey, UInt8).bytes(), grey.bytes()));
    }
}

TEST_CASE("image_typed_pixels", "[image]") {
    // 2 x 3 RGB, 16-bit: pixel (row, column) = (row, column, 7).
    image::Pixels<uint16_t> pixels(2, 3, 3);
    for (zipper::index_type r = 0; r < 2; ++r) {
        for (zipper::index_type c = 0; c < 3; ++c) {
            pixels(r, c, 0) = static_cast<uint16_t>(r);
            pixels(r, c, 1) = static_cast<uint16_t>(c);
            pixels(r, c, 2) = 7;
        }
    }
    image::Image image(std::move(pixels), image::Channels::Rgb);
    CHECK(image.component() == image::ComponentType::UInt16);
    CHECK(image.height() == 2);
    CHECK(image.width() == 3);
    CHECK(image.bytes().size() == 2 * 3 * 3 * sizeof(uint16_t));

    // Typed access is a zipper array; the wrong type gives nothing.
    CHECK_FALSE(image.pixels<float>());
    const auto *typed = image.pixels<uint16_t>();
    REQUIRE(typed);
    CHECK((*typed)(1, 2, 1) == 2);
    CHECK(zipper::as_array(typed->over<0, 1>().sum())(2) == 6 * 7);

    // A channel's plane.
    const auto green = image.channel<uint16_t>(1);
    REQUIRE(green);
    CHECK((*green)(0, 2) == 2);
    CHECK(green->sum() == 2 * (0 + 1 + 2));

    // visit dispatches on the stored type.
    const auto total = image.visit([](const auto &p) { return static_cast<double>(p.sum()); });
    CHECK(total == (0 + 0 + 1 + 1 + 1 + 0) + 2 * (0 + 1 + 2) + 6 * 7);

    // Writes through pixels<T>() are seen by the views.
    (*image.pixels<uint16_t>())(0, 0, 2) = 9;
    CHECK((*image.view<uint16_t, 3>())[0, 0, 2] == 9);

    // New images are zero-filled.
    const image::Image blank(2, 2, image::Channels::GreyAlpha, image::ComponentType::Float32);
    CHECK(blank.pixels<float>()->sum() == 0.0f);

    CHECK_THROWS_AS(image::Image(image::Pixels<float>(1, 1, 3), image::Channels::Rgba),
                    std::invalid_argument);
}
