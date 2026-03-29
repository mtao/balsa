#include "balsa/visualization/vulkan/texture.hpp"
#include "balsa/visualization/vulkan/film.hpp"

#include <cstring>
#include <spdlog/spdlog.h>
#include <stdexcept>

namespace balsa::visualization::vulkan {

// ── Helpers ─────────────────────────────────────────────────────────

auto VulkanTexture::vk_format() const -> vk::Format {
    switch (_format) {
    case Format::RGBA8:
        return vk::Format::eR8G8B8A8Unorm;
    case Format::RGBAF32:
        return vk::Format::eR32G32B32A32Sfloat;
    }
    return vk::Format::eR8G8B8A8Unorm;
}

auto VulkanTexture::bytes_per_pixel() const -> size_t {
    switch (_format) {
    case Format::RGBA8:
        return 4;
    case Format::RGBAF32:
        return 16;
    }
    return 4;
}

// ── Lifecycle ───────────────────────────────────────────────────────

VulkanTexture::~VulkanTexture() { release(); }

VulkanTexture::VulkanTexture(VulkanTexture &&o) noexcept
  : _device(o._device), _film(o._film), _image(o._image), _memory(o._memory),
    _image_view(o._image_view), _sampler(o._sampler),
    _staging_buffer(o._staging_buffer), _staging_memory(o._staging_memory),
    _staging_size(o._staging_size), _width(o._width), _height(o._height),
    _format(o._format) {
    o._device = vk::Device{};
    o._film = nullptr;
    o._image = vk::Image{};
    o._memory = vk::DeviceMemory{};
    o._image_view = vk::ImageView{};
    o._sampler = vk::Sampler{};
    o._staging_buffer = vk::Buffer{};
    o._staging_memory = vk::DeviceMemory{};
    o._staging_size = 0;
    o._width = 0;
    o._height = 0;
}

auto VulkanTexture::operator=(VulkanTexture &&o) noexcept -> VulkanTexture & {
    if (this != &o) {
        release();
        _device = o._device;
        _film = o._film;
        _image = o._image;
        _memory = o._memory;
        _image_view = o._image_view;
        _sampler = o._sampler;
        _staging_buffer = o._staging_buffer;
        _staging_memory = o._staging_memory;
        _staging_size = o._staging_size;
        _width = o._width;
        _height = o._height;
        _format = o._format;
        o._device = vk::Device{};
        o._film = nullptr;
        o._image = vk::Image{};
        o._memory = vk::DeviceMemory{};
        o._image_view = vk::ImageView{};
        o._sampler = vk::Sampler{};
        o._staging_buffer = vk::Buffer{};
        o._staging_memory = vk::DeviceMemory{};
        o._staging_size = 0;
        o._width = 0;
        o._height = 0;
    }
    return *this;
}

auto VulkanTexture::release() -> void {
    if (!_device) return;

    if (_sampler) {
        _device.destroySampler(_sampler);
        _sampler = vk::Sampler{};
    }
    if (_image_view) {
        _device.destroyImageView(_image_view);
        _image_view = vk::ImageView{};
    }
    if (_image) {
        _device.destroyImage(_image);
        _image = vk::Image{};
    }
    if (_memory) {
        _device.freeMemory(_memory);
        _memory = vk::DeviceMemory{};
    }
    if (_staging_buffer) {
        _device.destroyBuffer(_staging_buffer);
        _staging_buffer = vk::Buffer{};
    }
    if (_staging_memory) {
        _device.freeMemory(_staging_memory);
        _staging_memory = vk::DeviceMemory{};
    }

    _staging_size = 0;
    _width = 0;
    _height = 0;
}

// ── Create ──────────────────────────────────────────────────────────

auto VulkanTexture::create(Film &film,
                           uint32_t width,
                           uint32_t height,
                           Format format) -> void {
    release();

    _device = film.device();
    _film = &film;
    _width = width;
    _height = height;
    _format = format;

    // 1. Create the VkImage (device-local, transfer-dst + sampled).
    vk::ImageCreateInfo image_ci;
    image_ci.setImageType(vk::ImageType::e2D);
    image_ci.setFormat(vk_format());
    image_ci.setExtent(vk::Extent3D{width, height, 1});
    image_ci.setMipLevels(1);
    image_ci.setArrayLayers(1);
    image_ci.setSamples(vk::SampleCountFlagBits::e1);
    image_ci.setTiling(vk::ImageTiling::eOptimal);
    image_ci.setUsage(vk::ImageUsageFlagBits::eTransferDst
                      | vk::ImageUsageFlagBits::eSampled);
    image_ci.setSharingMode(vk::SharingMode::eExclusive);
    image_ci.setInitialLayout(vk::ImageLayout::eUndefined);

    _image = _device.createImage(image_ci);

    // 2. Allocate device-local memory and bind.
    auto mem_reqs = _device.getImageMemoryRequirements(_image);

    vk::MemoryAllocateInfo alloc_info;
    alloc_info.setAllocationSize(mem_reqs.size);
    alloc_info.setMemoryTypeIndex(film.find_memory_type(
        mem_reqs.memoryTypeBits, vk::MemoryPropertyFlagBits::eDeviceLocal));

    _memory = _device.allocateMemory(alloc_info);
    _device.bindImageMemory(_image, _memory, 0);

    // 3. Create image view.
    vk::ImageViewCreateInfo view_ci;
    view_ci.setImage(_image);
    view_ci.setViewType(vk::ImageViewType::e2D);
    view_ci.setFormat(vk_format());
    view_ci.subresourceRange.setAspectMask(vk::ImageAspectFlagBits::eColor);
    view_ci.subresourceRange.setBaseMipLevel(0);
    view_ci.subresourceRange.setLevelCount(1);
    view_ci.subresourceRange.setBaseArrayLayer(0);
    view_ci.subresourceRange.setLayerCount(1);

    _image_view = _device.createImageView(view_ci);

    // 4. Create sampler (linear filtering, clamp to edge).
    vk::SamplerCreateInfo sampler_ci;
    sampler_ci.setMagFilter(vk::Filter::eLinear);
    sampler_ci.setMinFilter(vk::Filter::eLinear);
    sampler_ci.setAddressModeU(vk::SamplerAddressMode::eClampToEdge);
    sampler_ci.setAddressModeV(vk::SamplerAddressMode::eClampToEdge);
    sampler_ci.setAddressModeW(vk::SamplerAddressMode::eClampToEdge);
    sampler_ci.setAnisotropyEnable(VK_FALSE);
    sampler_ci.setMaxAnisotropy(1.0f);
    sampler_ci.setBorderColor(vk::BorderColor::eIntOpaqueBlack);
    sampler_ci.setUnnormalizedCoordinates(VK_FALSE);
    sampler_ci.setCompareEnable(VK_FALSE);
    sampler_ci.setMipmapMode(vk::SamplerMipmapMode::eLinear);
    sampler_ci.setMipLodBias(0.0f);
    sampler_ci.setMinLod(0.0f);
    sampler_ci.setMaxLod(0.0f);

    _sampler = _device.createSampler(sampler_ci);

    // 5. Persistent staging buffer (host-visible + host-coherent).
    _staging_size =
        static_cast<vk::DeviceSize>(width) * height * bytes_per_pixel();

    vk::BufferCreateInfo staging_ci;
    staging_ci.setSize(_staging_size);
    staging_ci.setUsage(vk::BufferUsageFlagBits::eTransferSrc);
    staging_ci.setSharingMode(vk::SharingMode::eExclusive);

    _staging_buffer = _device.createBuffer(staging_ci);

    auto staging_reqs = _device.getBufferMemoryRequirements(_staging_buffer);

    vk::MemoryAllocateInfo staging_alloc;
    staging_alloc.setAllocationSize(staging_reqs.size);
    staging_alloc.setMemoryTypeIndex(
        film.find_memory_type(staging_reqs.memoryTypeBits,
                              vk::MemoryPropertyFlagBits::eHostVisible
                                  | vk::MemoryPropertyFlagBits::eHostCoherent));

    _staging_memory = _device.allocateMemory(staging_alloc);
    _device.bindBufferMemory(_staging_buffer, _staging_memory, 0);

    spdlog::info("VulkanTexture: created {}x{} ({} bpp)",
                 width,
                 height,
                 bytes_per_pixel());
}

// ── Layout transition ───────────────────────────────────────────────

auto VulkanTexture::transition_layout(vk::CommandBuffer cmd,
                                      vk::ImageLayout old_layout,
                                      vk::ImageLayout new_layout) -> void {
    vk::ImageMemoryBarrier barrier;
    barrier.setOldLayout(old_layout);
    barrier.setNewLayout(new_layout);
    barrier.setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED);
    barrier.setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED);
    barrier.setImage(_image);
    barrier.subresourceRange.setAspectMask(vk::ImageAspectFlagBits::eColor);
    barrier.subresourceRange.setBaseMipLevel(0);
    barrier.subresourceRange.setLevelCount(1);
    barrier.subresourceRange.setBaseArrayLayer(0);
    barrier.subresourceRange.setLayerCount(1);

    vk::PipelineStageFlags src_stage;
    vk::PipelineStageFlags dst_stage;

    if (old_layout == vk::ImageLayout::eUndefined
        && new_layout == vk::ImageLayout::eTransferDstOptimal) {
        barrier.setSrcAccessMask(vk::AccessFlags{});
        barrier.setDstAccessMask(vk::AccessFlagBits::eTransferWrite);
        src_stage = vk::PipelineStageFlagBits::eTopOfPipe;
        dst_stage = vk::PipelineStageFlagBits::eTransfer;
    } else if (old_layout == vk::ImageLayout::eTransferDstOptimal
               && new_layout == vk::ImageLayout::eShaderReadOnlyOptimal) {
        barrier.setSrcAccessMask(vk::AccessFlagBits::eTransferWrite);
        barrier.setDstAccessMask(vk::AccessFlagBits::eShaderRead);
        src_stage = vk::PipelineStageFlagBits::eTransfer;
        dst_stage = vk::PipelineStageFlagBits::eFragmentShader;
    } else if (old_layout == vk::ImageLayout::eShaderReadOnlyOptimal
               && new_layout == vk::ImageLayout::eTransferDstOptimal) {
        barrier.setSrcAccessMask(vk::AccessFlagBits::eShaderRead);
        barrier.setDstAccessMask(vk::AccessFlagBits::eTransferWrite);
        src_stage = vk::PipelineStageFlagBits::eFragmentShader;
        dst_stage = vk::PipelineStageFlagBits::eTransfer;
    } else {
        throw std::runtime_error(
            "VulkanTexture: unsupported layout transition");
    }

    cmd.pipelineBarrier(src_stage, dst_stage, {}, {}, {}, {barrier});
}

// ── One-shot command buffer ─────────────────────────────────────────

auto VulkanTexture::one_shot_command(
    Film &film,
    std::function<void(vk::CommandBuffer)> record_fn) -> void {
    vk::CommandBufferAllocateInfo cmd_alloc;
    cmd_alloc.setCommandPool(film.graphics_command_pool());
    cmd_alloc.setLevel(vk::CommandBufferLevel::ePrimary);
    cmd_alloc.setCommandBufferCount(1);

    auto cmd_buffers = _device.allocateCommandBuffers(cmd_alloc);
    vk::CommandBuffer cmd = cmd_buffers[0];

    vk::CommandBufferBeginInfo begin_info;
    begin_info.setFlags(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
    cmd.begin(begin_info);

    record_fn(cmd);

    cmd.end();

    vk::SubmitInfo submit_info;
    submit_info.setCommandBufferCount(1);
    submit_info.setPCommandBuffers(&cmd);

    vk::Queue queue = film.graphics_queue();
    queue.submit(submit_info);
    queue.waitIdle();

    _device.freeCommandBuffers(film.graphics_command_pool(), cmd);
}

// ── Full upload ─────────────────────────────────────────────────────

auto VulkanTexture::upload(Film &film, const void *pixels, size_t byte_count) -> void {
    if (!is_valid()) {
        throw std::runtime_error("VulkanTexture::upload: texture not created");
    }

    size_t expected = static_cast<size_t>(_width) * _height * bytes_per_pixel();
    if (byte_count < expected) {
        throw std::runtime_error("VulkanTexture::upload: insufficient data");
    }

    // Copy into staging buffer.
    void *mapped = _device.mapMemory(_staging_memory, 0, _staging_size);
    std::memcpy(mapped, pixels, expected);
    _device.unmapMemory(_staging_memory);

    // Record one-shot commands: transition, copy, transition.
    one_shot_command(film, [&](vk::CommandBuffer cmd) {
        // UNDEFINED -> TRANSFER_DST
        transition_layout(cmd,
                          vk::ImageLayout::eUndefined,
                          vk::ImageLayout::eTransferDstOptimal);

        // Copy staging buffer -> image.
        vk::BufferImageCopy region;
        region.setBufferOffset(0);
        region.setBufferRowLength(0); // tightly packed
        region.setBufferImageHeight(0); // tightly packed
        region.imageSubresource.setAspectMask(vk::ImageAspectFlagBits::eColor);
        region.imageSubresource.setMipLevel(0);
        region.imageSubresource.setBaseArrayLayer(0);
        region.imageSubresource.setLayerCount(1);
        region.setImageOffset({0, 0, 0});
        region.setImageExtent({_width, _height, 1});

        cmd.copyBufferToImage(_staging_buffer,
                              _image,
                              vk::ImageLayout::eTransferDstOptimal,
                              {region});

        // TRANSFER_DST -> SHADER_READ_ONLY
        transition_layout(cmd,
                          vk::ImageLayout::eTransferDstOptimal,
                          vk::ImageLayout::eShaderReadOnlyOptimal);
    });
}

// ── Partial region update ───────────────────────────────────────────

auto VulkanTexture::update_region(Film &film,
                                  uint32_t x,
                                  uint32_t y,
                                  uint32_t w,
                                  uint32_t h,
                                  const void *pixels,
                                  size_t byte_count) -> void {
    if (!is_valid()) {
        throw std::runtime_error(
            "VulkanTexture::update_region: texture not created");
    }

    size_t bpp = bytes_per_pixel();
    size_t expected = static_cast<size_t>(w) * h * bpp;
    if (byte_count < expected) {
        throw std::runtime_error(
            "VulkanTexture::update_region: insufficient data");
    }

    // Copy region data into the staging buffer at offset 0.
    void *mapped = _device.mapMemory(_staging_memory, 0, expected);
    std::memcpy(mapped, pixels, expected);
    _device.unmapMemory(_staging_memory);

    one_shot_command(film, [&](vk::CommandBuffer cmd) {
        // SHADER_READ_ONLY -> TRANSFER_DST
        transition_layout(cmd,
                          vk::ImageLayout::eShaderReadOnlyOptimal,
                          vk::ImageLayout::eTransferDstOptimal);

        // Copy staging -> sub-region of image.
        vk::BufferImageCopy region;
        region.setBufferOffset(0);
        region.setBufferRowLength(w);
        region.setBufferImageHeight(h);
        region.imageSubresource.setAspectMask(vk::ImageAspectFlagBits::eColor);
        region.imageSubresource.setMipLevel(0);
        region.imageSubresource.setBaseArrayLayer(0);
        region.imageSubresource.setLayerCount(1);
        region.setImageOffset(
            {static_cast<int32_t>(x), static_cast<int32_t>(y), 0});
        region.setImageExtent({w, h, 1});

        cmd.copyBufferToImage(_staging_buffer,
                              _image,
                              vk::ImageLayout::eTransferDstOptimal,
                              {region});

        // TRANSFER_DST -> SHADER_READ_ONLY
        transition_layout(cmd,
                          vk::ImageLayout::eTransferDstOptimal,
                          vk::ImageLayout::eShaderReadOnlyOptimal);
    });
}

} // namespace balsa::visualization::vulkan
