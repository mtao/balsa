#pragma once
#include <balsa/image/Image.hpp>
#include <balsa/scene_graph/Texture.hpp>

#include <cstdint>
#include <vulkan/vulkan.hpp>

namespace balsa::visualization::vulkan {

class Film;

// How an image is stored on the GPU: always four channels; sRGB only as
// 8-bit (Vulkan has no 16-bit or float sRGB formats).
struct TextureFormat {
    vk::Format format;
    image::ComponentType component;
};
auto texture_format(const image::Image &image, scene_graph::ColorSpace color_space)
    -> TextureFormat;

// Levels in a full mip chain of a width x height image.
auto mip_level_count(uint32_t width, uint32_t height) -> uint32_t;

// An immutable, device-local, sampled 2D image with mipmaps, generated on
// the GPU when its format supports linear blits.  Images are expanded to
// RGBA by their layout; those of unknown layout must have four channels.
// Samplers are separate.
// Move-only; destruction frees the GPU objects, so retire it through the
// Film while frames may still use it.
class SampledImage {
  public:
    SampledImage() = default;
    SampledImage(Film &film, const image::Image &image, scene_graph::ColorSpace color_space);
    ~SampledImage();

    SampledImage(const SampledImage &) = delete;
    auto operator=(const SampledImage &) -> SampledImage & = delete;
    SampledImage(SampledImage &&other) noexcept;
    auto operator=(SampledImage &&other) noexcept -> SampledImage &;

    [[nodiscard]] auto view() const -> vk::ImageView { return m_view; }
    [[nodiscard]] auto format() const -> vk::Format { return m_format; }
    [[nodiscard]] auto mip_levels() const -> uint32_t { return m_mip_levels; }
    [[nodiscard]] auto is_valid() const -> bool { return m_image != vk::Image{}; }

    auto release() -> void;

  private:
    vk::Device m_device;
    vk::Image m_image;
    vk::DeviceMemory m_memory;
    vk::ImageView m_view;
    vk::Format m_format = vk::Format::eUndefined;
    uint32_t m_mip_levels = 0;
};

} // namespace balsa::visualization::vulkan
