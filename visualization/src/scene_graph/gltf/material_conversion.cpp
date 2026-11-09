#include "balsa/visualization/scene_graph/gltf/material_conversion.hpp"
#include "balsa/visualization/scene_graph/gltf/detail/LoadErrors.hpp"

#include <balsa/image/decode.hpp>

#include <zipper/VectorBase.hpp>

#include <format>
#include <ranges>

namespace balsa::visualization::gltf {

namespace sg = balsa::scene_graph;

namespace {
    // Converts textures and decodes images on first use.
    class MaterialConverter {
      public:
        explicit MaterialConverter(const Document &document)
          : m_document(document),
            m_images(document.manifest().images.size()),
            m_textures(document.manifest().textures.size()) {}

        auto material(std::size_t index) -> std::shared_ptr<sg::Material> {
            const auto &source = m_document.manifest().materials[index];
            const auto context = std::format("material {}", index);
            auto material = std::make_shared<sg::Material>();
            material->name = source.name.value_or("");

            if (const auto &pbr = source.pbrMetallicRoughness) {
                material->base_color = sg::Vec4f(::zipper::VectorBase(pbr->baseColorFactor));
                material->metallic = pbr->metallicFactor;
                material->roughness = pbr->roughnessFactor;
                material->base_color_texture = texture(context, pbr->baseColorTexture);
                material->metallic_roughness_texture =
                    texture(context, pbr->metallicRoughnessTexture);
            }
            if (const auto &normal = source.normalTexture) {
                material->normal_texture = texture(context, *normal);
                material->normal_scale = normal->scale;
            }
            if (const auto &occlusion = source.occlusionTexture) {
                material->occlusion_texture = texture(context, *occlusion);
                material->occlusion_strength = occlusion->strength;
            }
            material->emissive = sg::Vec3f(::zipper::VectorBase(source.emissiveFactor));
            material->emissive_texture = texture(context, source.emissiveTexture);

            material->alpha_mode = source.alphaMode == AlphaMode::MASK    ? sg::AlphaMode::Mask
                                   : source.alphaMode == AlphaMode::BLEND ? sg::AlphaMode::Blend
                                                                          : sg::AlphaMode::Opaque;
            material->alpha_cutoff = source.alphaCutoff;
            material->double_sided = source.doubleSided;
            return material;
        }

        auto errors() && -> detail::LoadErrors { return std::move(m_errors); }

      private:
        // Any glTF texture info: index and texCoord.
        template <typename Info>
        auto texture(std::string_view context, const std::optional<Info> &info)
            -> std::optional<sg::MaterialTexture> {
            if (!info) return std::nullopt;
            return texture(context, *info);
        }
        template <typename Info>
        auto texture(std::string_view context, const Info &info)
            -> std::optional<sg::MaterialTexture> {
            const auto index = static_cast<std::size_t>(info.index);
            if (info.index < 0 || index >= m_textures.size()) {
                m_errors.add(LoadError::Code::InvalidGltf,
                             std::format("{}: no texture {}", context, info.index));
                return std::nullopt;
            }
            if (info.texCoord < 0 || info.texCoord > 255) {
                m_errors.add(LoadError::Code::InvalidGltf,
                             std::format("{}: invalid texCoord {}", context, info.texCoord));
                return std::nullopt;
            }
            auto converted = texture(index);
            if (!converted) return std::nullopt;
            return sg::MaterialTexture{std::move(converted), static_cast<uint8_t>(info.texCoord)};
        }

        auto texture(std::size_t index) -> std::shared_ptr<const sg::Texture> {
            auto &cached = m_textures[index];
            if (cached) return *cached;
            cached.emplace();
            const auto &manifest = m_document.manifest();
            const auto &source = manifest.textures[index];
            const auto context = std::format("texture {}", index);
            if (!source.source) return nullptr;

            auto converted = std::make_shared<sg::Texture>();
            converted->name = source.name.value_or("");
            if (source.sampler) {
                const auto sampler = static_cast<std::size_t>(*source.sampler);
                if (*source.sampler < 0 || sampler >= manifest.samplers.size()) {
                    m_errors.add(LoadError::Code::InvalidGltf,
                                 std::format("{}: no sampler {}", context, *source.sampler));
                    return nullptr;
                }
                converted->sampler = to_sampler(manifest.samplers[sampler]);
            }
            const auto image_index = static_cast<std::size_t>(*source.source);
            if (*source.source < 0 || image_index >= m_images.size()) {
                m_errors.add(LoadError::Code::InvalidGltf,
                             std::format("{}: no image {}", context, *source.source));
                return nullptr;
            }
            converted->image = image(image_index);
            if (!converted->image) return nullptr;
            *cached = std::move(converted);
            return *cached;
        }

        auto image(std::size_t index) -> std::shared_ptr<const image::Image> {
            auto &cached = m_images[index];
            if (cached) return *cached;
            cached.emplace();
            auto decoded = image::decode(m_document.image(index));
            if (!decoded) {
                m_errors.add(LoadError::Code::InvalidGltf,
                             std::format("image {}: {}", index, decoded.error().message));
                return nullptr;
            }
            *cached = std::make_shared<const image::Image>(std::move(*decoded));
            return *cached;
        }

        const Document &m_document;
        // Converted on first use; a null entry failed or has no source.
        std::vector<std::optional<std::shared_ptr<const image::Image>>> m_images;
        std::vector<std::optional<std::shared_ptr<const sg::Texture>>> m_textures;
        detail::LoadErrors m_errors;
    };
} // namespace

auto to_sampler(const Sampler &sampler) -> sg::Sampler {
    const auto wrap = [](WrapMode mode) {
        switch (mode) {
        case WrapMode::CLAMP_TO_EDGE: return sg::TextureWrap::ClampToEdge;
        case WrapMode::MIRRORED_REPEAT: return sg::TextureWrap::MirroredRepeat;
        case WrapMode::REPEAT: break;
        }
        return sg::TextureWrap::Repeat;
    };
    using enum sg::TextureFilter;
    sg::Sampler result{
        .magnification = sampler.magFilter == MagFilter::NEAREST ? Nearest : Linear,
        .wrap_u = wrap(sampler.wrapS),
        .wrap_v = wrap(sampler.wrapT),
    };
    if (sampler.minFilter) {
        switch (*sampler.minFilter) {
        case MinFilter::NEAREST: result.minification = Nearest; result.mipmap = std::nullopt; break;
        case MinFilter::LINEAR: result.minification = Linear; result.mipmap = std::nullopt; break;
        case MinFilter::NEAREST_MIPMAP_NEAREST: result.minification = Nearest; result.mipmap = Nearest; break;
        case MinFilter::LINEAR_MIPMAP_NEAREST: result.minification = Linear; result.mipmap = Nearest; break;
        case MinFilter::NEAREST_MIPMAP_LINEAR: result.minification = Nearest; result.mipmap = Linear; break;
        case MinFilter::LINEAR_MIPMAP_LINEAR: result.minification = Linear; result.mipmap = Linear; break;
        }
    }
    return result;
}

auto to_materials(const Document &document)
    -> std::expected<std::vector<std::shared_ptr<sg::Material>>, LoadError> {
    MaterialConverter converter(document);
    auto materials = std::views::iota(std::size_t{0}, document.manifest().materials.size())
                     | std::views::transform([&](std::size_t i) { return converter.material(i); })
                     | std::ranges::to<std::vector>();
    if (auto errors = std::move(converter).errors(); !errors.empty()) {
        return std::unexpected(std::move(errors).combined());
    }
    return materials;
}

} // namespace balsa::visualization::gltf
