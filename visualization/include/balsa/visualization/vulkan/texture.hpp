#if !defined(BALSA_VISUALIZATION_VULKAN_TEXTURE_HPP)
#define BALSA_VISUALIZATION_VULKAN_TEXTURE_HPP

#include <cstddef>
#include <cstdint>
#include <functional>
#include <vulkan/vulkan.hpp>

namespace balsa::visualization::vulkan {

class Film;

// ── VulkanTexture ───────────────────────────────────────────────────
//
// RAII wrapper for a Vulkan texture (VkImage + VkImageView + VkSampler
// + VkDeviceMemory).  Supports full and partial (rectangular region)
// uploads through a staging buffer.
//
// Designed for image viewer use (RGBA8 or RGBAF32 textures) but is a
// general-purpose reusable primitive (future: environment maps,
// texture-mapped meshes, LUT colormaps).
//
// Non-copyable, movable.  Destroying or moving-from releases the
// Vulkan resources.

class VulkanTexture {
  public:
    enum class Format { RGBA8, RGBAF32 };

    VulkanTexture() = default;
    ~VulkanTexture();

    // Non-copyable
    VulkanTexture(const VulkanTexture &) = delete;
    VulkanTexture &operator=(const VulkanTexture &) = delete;

    // Movable
    VulkanTexture(VulkanTexture &&) noexcept;
    VulkanTexture &operator=(VulkanTexture &&) noexcept;

    // Create GPU resources (image, memory, view, sampler).
    // The image is created device-local for optimal sampling.
    // A persistent staging buffer is allocated for uploads.
    void create(Film &film, uint32_t width, uint32_t height, Format format);

    // Upload full image data.  Stages through a host-visible buffer,
    // copies to device-local memory via a one-shot command buffer.
    // The image layout is transitioned:
    //   UNDEFINED -> TRANSFER_DST_OPTIMAL -> SHADER_READ_ONLY_OPTIMAL
    void upload(Film &film, const void *pixels, size_t byte_count);

    // Partial update: upload a rectangular sub-region.
    // For progressive rendering — avoids re-uploading the entire image.
    // Layout: SHADER_READ_ONLY -> TRANSFER_DST -> SHADER_READ_ONLY.
    void update_region(Film &film,
                       uint32_t x,
                       uint32_t y,
                       uint32_t w,
                       uint32_t h,
                       const void *pixels,
                       size_t byte_count);

    // Accessors for descriptor set binding
    vk::ImageView image_view() const { return _image_view; }
    vk::Sampler sampler() const { return _sampler; }

    uint32_t width() const { return _width; }
    uint32_t height() const { return _height; }
    Format format() const { return _format; }

    // Release all GPU resources.  Safe to call multiple times.
    void release();
    bool is_valid() const { return _image != vk::Image{}; }

  private:
    // Record and execute a layout transition barrier.
    void transition_layout(vk::CommandBuffer cmd,
                           vk::ImageLayout old_layout,
                           vk::ImageLayout new_layout);

    // Execute a one-shot command buffer (record -> submit -> waitIdle).
    // The callback receives the command buffer to record into.
    void one_shot_command(Film &film,
                          std::function<void(vk::CommandBuffer)> record_fn);

    // Return the VkFormat corresponding to our Format enum.
    vk::Format vk_format() const;

    // Bytes per pixel for the current format.
    size_t bytes_per_pixel() const;

    vk::Device _device;
    Film *_film = nullptr;
    vk::Image _image;
    vk::DeviceMemory _memory;
    vk::ImageView _image_view;
    vk::Sampler _sampler;

    // Persistent staging buffer for uploads.
    // Sized to hold a full image.  Re-created if the texture is
    // recreated with different dimensions.
    vk::Buffer _staging_buffer;
    vk::DeviceMemory _staging_memory;
    vk::DeviceSize _staging_size = 0;

    uint32_t _width = 0, _height = 0;
    Format _format = Format::RGBA8;
};

} // namespace balsa::visualization::vulkan

#endif
