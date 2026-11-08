#pragma once
#include "sampled_image.hpp"

#include <balsa/image/Image.hpp>
#include <balsa/scene_graph/Texture.hpp>

#include <cstddef>
#include <memory>
#include <optional>
#include <unordered_map>
#include <utility>
#include <vector>
#include <vulkan/vulkan.hpp>

namespace balsa::visualization::vulkan {

class Film;

// Vulkan settings for `sampler`.  Without mipmapping only the base level is
// sampled.
auto sampler_create_info(const scene_graph::Sampler &sampler) -> vk::SamplerCreateInfo;

// GPU images and samplers for material textures, shared between the
// materials using them.  Calls must come from the render thread, outside
// command recording for uploads.
class TextureCache {
  public:
    TextureCache() = default;
    ~TextureCache();
    TextureCache(const TextureCache &) = delete;
    auto operator=(const TextureCache &) -> TextureCache & = delete;

    // `image` on the GPU in `color_space`, uploaded on first use and kept
    // while `image` is alive.
    auto image(Film &film, const std::shared_ptr<const image::Image> &image,
               scene_graph::ColorSpace color_space) -> const SampledImage &;
    auto sampler(Film &film, const scene_graph::Sampler &sampler) -> vk::Sampler;

    // 1 x 1 stand-ins for empty material slots: white leaves factors
    // unchanged; a flat normal points along the surface normal.
    auto white(Film &film) -> const SampledImage &;
    auto flat_normal(Film &film) -> const SampledImage &;

    // Retires the GPU copies of images that no longer exist.
    auto collect(Film &film) -> void;
    // Frees everything; the GPU must be idle.
    auto release() -> void;

  private:
    struct Key {
        const image::Image *image;
        scene_graph::ColorSpace color_space;
        auto operator==(const Key &) const -> bool = default;
    };
    struct KeyHash {
        auto operator()(const Key &key) const -> std::size_t;
    };
    struct Entry {
        // Detects a new image at a reused address.
        std::weak_ptr<const image::Image> source;
        SampledImage gpu;
    };

    vk::Device m_device;
    std::unordered_map<Key, Entry, KeyHash> m_images;
    std::vector<std::pair<scene_graph::Sampler, vk::Sampler>> m_samplers;
    std::optional<SampledImage> m_white;
    std::optional<SampledImage> m_flat_normal;
};

} // namespace balsa::visualization::vulkan
