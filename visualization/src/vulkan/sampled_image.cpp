#include "balsa/visualization/vulkan/sampled_image.hpp"
#include "balsa/visualization/vulkan/buffer.hpp"
#include "balsa/visualization/vulkan/film.hpp"
#include "balsa/visualization/vulkan/one_shot_command.hpp"

#include <balsa/image/convert.hpp>

#include <algorithm>
#include <bit>
#include <stdexcept>
#include <utility>

namespace balsa::visualization::vulkan {

namespace {
    auto color_range(uint32_t base_level, uint32_t levels) -> vk::ImageSubresourceRange {
        return {vk::ImageAspectFlagBits::eColor, base_level, levels, 0, 1};
    }

    // Barrier moving `levels` levels from `base_level` between layouts.
    auto barrier(vk::Image image, uint32_t base_level, uint32_t levels,
                 vk::ImageLayout from, vk::ImageLayout to, vk::AccessFlags src_access,
                 vk::AccessFlags dst_access) -> vk::ImageMemoryBarrier {
        vk::ImageMemoryBarrier b;
        b.setImage(image);
        b.setSubresourceRange(color_range(base_level, levels));
        b.setOldLayout(from);
        b.setNewLayout(to);
        b.setSrcAccessMask(src_access);
        b.setDstAccessMask(dst_access);
        b.setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED);
        b.setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED);
        return b;
    }

    // Whether `format` can be blitted with linear filtering, to make mipmaps.
    auto can_generate_mipmaps(Film &film, vk::Format format) -> bool {
        const auto features = film.physical_device().getFormatProperties(format).optimalTilingFeatures;
        const auto needed = vk::FormatFeatureFlagBits::eBlitSrc | vk::FormatFeatureFlagBits::eBlitDst
                            | vk::FormatFeatureFlagBits::eSampledImageFilterLinear;
        return (features & needed) == needed;
    }

    // Records level 0's copy from `staging` and the mip chain; leaves every
    // level shader-readable.
    void record_upload(vk::CommandBuffer cmd, vk::Buffer staging, vk::Image image, uint32_t width,
                       uint32_t height, uint32_t levels) {
        using L = vk::ImageLayout;
        using A = vk::AccessFlagBits;
        using S = vk::PipelineStageFlagBits;
        cmd.pipelineBarrier(S::eTopOfPipe, S::eTransfer, {}, {}, {},
                            barrier(image, 0, levels, L::eUndefined, L::eTransferDstOptimal, {},
                                    A::eTransferWrite));
        vk::BufferImageCopy copy;
        copy.setImageSubresource({vk::ImageAspectFlagBits::eColor, 0, 0, 1});
        copy.setImageExtent({width, height, 1});
        cmd.copyBufferToImage(staging, image, L::eTransferDstOptimal, copy);

        // Each level is blitted from the previous one, which is then final.
        auto w = static_cast<int32_t>(width);
        auto h = static_cast<int32_t>(height);
        for (uint32_t level = 1; level < levels; ++level) {
            cmd.pipelineBarrier(S::eTransfer, S::eTransfer, {}, {}, {},
                                barrier(image, level - 1, 1, L::eTransferDstOptimal,
                                        L::eTransferSrcOptimal, A::eTransferWrite, A::eTransferRead));
            const auto next_w = std::max(w / 2, 1);
            const auto next_h = std::max(h / 2, 1);
            vk::ImageBlit blit;
            blit.setSrcSubresource({vk::ImageAspectFlagBits::eColor, level - 1, 0, 1});
            blit.setSrcOffsets({vk::Offset3D{0, 0, 0}, vk::Offset3D{w, h, 1}});
            blit.setDstSubresource({vk::ImageAspectFlagBits::eColor, level, 0, 1});
            blit.setDstOffsets({vk::Offset3D{0, 0, 0}, vk::Offset3D{next_w, next_h, 1}});
            cmd.blitImage(image, L::eTransferSrcOptimal, image, L::eTransferDstOptimal, blit,
                          vk::Filter::eLinear);
            cmd.pipelineBarrier(S::eTransfer, S::eFragmentShader, {}, {}, {},
                                barrier(image, level - 1, 1, L::eTransferSrcOptimal,
                                        L::eShaderReadOnlyOptimal, A::eTransferRead, A::eShaderRead));
            w = next_w;
            h = next_h;
        }
        cmd.pipelineBarrier(S::eTransfer, S::eFragmentShader, {}, {}, {},
                            barrier(image, levels - 1, 1, L::eTransferDstOptimal,
                                    L::eShaderReadOnlyOptimal, A::eTransferWrite, A::eShaderRead));
    }
} // namespace

auto texture_format(const image::Image &image, scene_graph::ColorSpace color_space)
    -> TextureFormat {
    using enum image::ComponentType;
    const bool srgb = color_space == scene_graph::ColorSpace::Srgb;
    switch (image.component()) {
    case Float32: return {vk::Format::eR32G32B32A32Sfloat, Float32};
    case UInt16:
        if (!srgb) return {vk::Format::eR16G16B16A16Unorm, UInt16};
        break;
    case UInt8: break;
    }
    return {srgb ? vk::Format::eR8G8B8A8Srgb : vk::Format::eR8G8B8A8Unorm, UInt8};
}

auto mip_level_count(uint32_t width, uint32_t height) -> uint32_t {
    return static_cast<uint32_t>(std::bit_width(std::max({width, height, 1u})));
}

SampledImage::SampledImage(Film &film, const image::Image &source,
                           scene_graph::ColorSpace color_space)
  : m_device(film.device()) {
    if (source.empty()) throw std::invalid_argument("SampledImage: empty image");
    const auto [format, component] = texture_format(source, color_space);
    // GPUs store four channels; plain-data images must already have them.
    if (!source.layout() && source.channels() != 4) {
        throw std::invalid_argument("SampledImage: images of unknown layout need four channels");
    }
    const auto rgba =
        source.layout() ? image::convert_channels(source, image::Channels::Rgba) : source;
    const auto pixels = image::convert_component(rgba, component);
    const auto width = static_cast<uint32_t>(pixels.width());
    const auto height = static_cast<uint32_t>(pixels.height());
    m_format = format;
    m_mip_levels = can_generate_mipmaps(film, format) ? mip_level_count(width, height) : 1;

    // Free what was created if a later step fails.
    try {
        vk::ImageCreateInfo info;
        info.setImageType(vk::ImageType::e2D);
        info.setFormat(format);
        info.setExtent({width, height, 1});
        info.setMipLevels(m_mip_levels);
        info.setArrayLayers(1);
        info.setSamples(vk::SampleCountFlagBits::e1);
        info.setTiling(vk::ImageTiling::eOptimal);
        info.setUsage(vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst
                      | vk::ImageUsageFlagBits::eSampled);
        info.setInitialLayout(vk::ImageLayout::eUndefined);
        m_image = m_device.createImage(info);

        const auto requirements = m_device.getImageMemoryRequirements(m_image);
        m_memory = m_device.allocateMemory(
            {requirements.size, film.find_memory_type(requirements.memoryTypeBits,
                                                      vk::MemoryPropertyFlagBits::eDeviceLocal)});
        m_device.bindImageMemory(m_image, m_memory, 0);

        VulkanBuffer staging(film, pixels.bytes().size(), vk::BufferUsageFlagBits::eTransferSrc,
                             vk::MemoryPropertyFlagBits::eHostVisible
                                 | vk::MemoryPropertyFlagBits::eHostCoherent);
        staging.upload(pixels.bytes());
        one_shot_command(film, [&](vk::CommandBuffer cmd) {
            record_upload(cmd, staging.buffer(), m_image, width, height, m_mip_levels);
        });

        vk::ImageViewCreateInfo view;
        view.setImage(m_image);
        view.setViewType(vk::ImageViewType::e2D);
        view.setFormat(format);
        view.setSubresourceRange(color_range(0, m_mip_levels));
        m_view = m_device.createImageView(view);
    } catch (...) {
        release();
        throw;
    }
}

SampledImage::~SampledImage() { release(); }

SampledImage::SampledImage(SampledImage &&other) noexcept
  : m_device(std::exchange(other.m_device, {})),
    m_image(std::exchange(other.m_image, {})),
    m_memory(std::exchange(other.m_memory, {})),
    m_view(std::exchange(other.m_view, {})),
    m_format(std::exchange(other.m_format, vk::Format::eUndefined)),
    m_mip_levels(std::exchange(other.m_mip_levels, 0)) {}

auto SampledImage::operator=(SampledImage &&other) noexcept -> SampledImage & {
    if (this != &other) {
        release();
        m_device = std::exchange(other.m_device, {});
        m_image = std::exchange(other.m_image, {});
        m_memory = std::exchange(other.m_memory, {});
        m_view = std::exchange(other.m_view, {});
        m_format = std::exchange(other.m_format, vk::Format::eUndefined);
        m_mip_levels = std::exchange(other.m_mip_levels, 0);
    }
    return *this;
}

auto SampledImage::release() -> void {
    if (!m_device) return;
    if (m_view) m_device.destroyImageView(m_view);
    if (m_image) m_device.destroyImage(m_image);
    if (m_memory) m_device.freeMemory(m_memory);
    m_view = vk::ImageView{};
    m_image = vk::Image{};
    m_memory = vk::DeviceMemory{};
    m_mip_levels = 0;
}

} // namespace balsa::visualization::vulkan
