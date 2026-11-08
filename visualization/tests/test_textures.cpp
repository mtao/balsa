#include <catch2/catch_test_macros.hpp>

#include <balsa/visualization/vulkan/sampled_image.hpp>
#include <balsa/visualization/vulkan/texture_cache.hpp>

namespace sg = balsa::scene_graph;
namespace vulkan = balsa::visualization::vulkan;
namespace image = balsa::image;

TEST_CASE("texture_format", "[vulkan][texture]") {
    using enum image::ComponentType;
    const auto format = [](image::ComponentType component, sg::ColorSpace color_space) {
        return vulkan::texture_format(image::Image(image::Image::Extents(1, 1, 3), component),
                                      color_space);
    };
    CHECK(format(UInt8, sg::ColorSpace::Srgb).format == vk::Format::eR8G8B8A8Srgb);
    CHECK(format(UInt8, sg::ColorSpace::Linear).format == vk::Format::eR8G8B8A8Unorm);
    CHECK(format(UInt16, sg::ColorSpace::Linear).format == vk::Format::eR16G16B16A16Unorm);
    // No 16-bit sRGB format: reduced to 8 bits.
    const auto wide_srgb = format(UInt16, sg::ColorSpace::Srgb);
    CHECK(wide_srgb.format == vk::Format::eR8G8B8A8Srgb);
    CHECK(wide_srgb.component == UInt8);
    CHECK(format(Float32, sg::ColorSpace::Srgb).format == vk::Format::eR32G32B32A32Sfloat);
}

TEST_CASE("mip_level_count", "[vulkan][texture]") {
    CHECK(vulkan::mip_level_count(1, 1) == 1);
    CHECK(vulkan::mip_level_count(2, 1) == 2);
    CHECK(vulkan::mip_level_count(256, 256) == 9);
    CHECK(vulkan::mip_level_count(300, 7) == 9);
}

TEST_CASE("sampler_create_info", "[vulkan][texture]") {
    const auto mipmapped = vulkan::sampler_create_info(sg::Sampler{
        .magnification = sg::TextureFilter::Nearest,
        .minification = sg::TextureFilter::Linear,
        .mipmap = sg::TextureFilter::Nearest,
        .wrap_u = sg::TextureWrap::ClampToEdge,
        .wrap_v = sg::TextureWrap::MirroredRepeat,
    });
    CHECK(mipmapped.magFilter == vk::Filter::eNearest);
    CHECK(mipmapped.minFilter == vk::Filter::eLinear);
    CHECK(mipmapped.mipmapMode == vk::SamplerMipmapMode::eNearest);
    CHECK(mipmapped.addressModeU == vk::SamplerAddressMode::eClampToEdge);
    CHECK(mipmapped.addressModeV == vk::SamplerAddressMode::eMirroredRepeat);
    CHECK(mipmapped.maxLod == VK_LOD_CLAMP_NONE);

    // Without mipmapping, only the base level.
    const auto base_only = vulkan::sampler_create_info(sg::Sampler{.mipmap = std::nullopt});
    CHECK(base_only.maxLod == 0.0f);
    CHECK(base_only.addressModeU == vk::SamplerAddressMode::eRepeat);
}
