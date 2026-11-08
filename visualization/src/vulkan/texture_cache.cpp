#include "balsa/visualization/vulkan/texture_cache.hpp"
#include "balsa/visualization/vulkan/film.hpp"

#include <algorithm>
#include <array>
#include <functional>

namespace balsa::visualization::vulkan {

namespace sg = balsa::scene_graph;

namespace {
    auto vk_filter(sg::TextureFilter filter) -> vk::Filter {
        return filter == sg::TextureFilter::Nearest ? vk::Filter::eNearest : vk::Filter::eLinear;
    }
    auto vk_address_mode(sg::TextureWrap wrap) -> vk::SamplerAddressMode {
        switch (wrap) {
        case sg::TextureWrap::ClampToEdge: return vk::SamplerAddressMode::eClampToEdge;
        case sg::TextureWrap::MirroredRepeat: return vk::SamplerAddressMode::eMirroredRepeat;
        case sg::TextureWrap::Repeat: break;
        }
        return vk::SamplerAddressMode::eRepeat;
    }

    // A 1 x 1 RGBA8 image of `rgba`, uploaded as linear.
    auto solid(Film &film, std::array<uint8_t, 4> rgba) -> SampledImage {
        image::Image pixel(1, 1, image::Channels::Rgba, image::ComponentType::UInt8);
        std::ranges::copy(std::as_bytes(std::span(rgba)), pixel.bytes().begin());
        return SampledImage(film, pixel, sg::ColorSpace::Linear);
    }
} // namespace

auto sampler_create_info(const sg::Sampler &sampler) -> vk::SamplerCreateInfo {
    vk::SamplerCreateInfo info;
    info.setMagFilter(vk_filter(sampler.magnification));
    info.setMinFilter(vk_filter(sampler.minification));
    info.setMipmapMode(sampler.mipmap == sg::TextureFilter::Nearest
                           ? vk::SamplerMipmapMode::eNearest
                           : vk::SamplerMipmapMode::eLinear);
    info.setAddressModeU(vk_address_mode(sampler.wrap_u));
    info.setAddressModeV(vk_address_mode(sampler.wrap_v));
    info.setAddressModeW(vk::SamplerAddressMode::eRepeat);
    info.setMinLod(0.0f);
    info.setMaxLod(sampler.mipmap ? VK_LOD_CLAMP_NONE : 0.0f);
    return info;
}

auto TextureCache::KeyHash::operator()(const Key &key) const -> std::size_t {
    return std::hash<const image::Image *>{}(key.image) ^ static_cast<std::size_t>(key.color_space);
}

TextureCache::~TextureCache() { release(); }

auto TextureCache::image(Film &film, const std::shared_ptr<const image::Image> &image,
                         sg::ColorSpace color_space) -> const SampledImage & {
    m_device = film.device();
    const Key key{image.get(), color_space};
    if (const auto it = m_images.find(key); it != m_images.end()) {
        if (it->second.source.lock() == image) return it->second.gpu;
        film.retire(std::move(it->second.gpu));
        m_images.erase(it);
    }
    return m_images.emplace(key, Entry{image, SampledImage(film, *image, color_space)})
        .first->second.gpu;
}

auto TextureCache::sampler(Film &film, const sg::Sampler &sampler) -> vk::Sampler {
    m_device = film.device();
    if (const auto it = std::ranges::find(m_samplers, sampler, &decltype(m_samplers)::value_type::first);
        it != m_samplers.end()) {
        return it->second;
    }
    return m_samplers.emplace_back(sampler, m_device.createSampler(sampler_create_info(sampler)))
        .second;
}

auto TextureCache::white(Film &film) -> const SampledImage & {
    if (!m_white) m_white = solid(film, {255, 255, 255, 255});
    return *m_white;
}

auto TextureCache::flat_normal(Film &film) -> const SampledImage & {
    if (!m_flat_normal) m_flat_normal = solid(film, {128, 128, 255, 255});
    return *m_flat_normal;
}

auto TextureCache::collect(Film &film) -> void {
    std::erase_if(m_images, [&](auto &entry) {
        if (!entry.second.source.expired()) return false;
        film.retire(std::move(entry.second.gpu));
        return true;
    });
}

auto TextureCache::release() -> void {
    m_images.clear();
    m_white.reset();
    m_flat_normal.reset();
    for (const auto &[description, sampler] : m_samplers) m_device.destroySampler(sampler);
    m_samplers.clear();
}

} // namespace balsa::visualization::vulkan
