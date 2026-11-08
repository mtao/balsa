#pragma once
#include <balsa/image/Image.hpp>

#include <cstdint>
#include <memory>
#include <optional>
#include <string>

namespace balsa::scene_graph {

// How a texture's colour values are encoded; set by the material slot
// using it (e.g. glTF base colour is sRGB, normals linear).
enum class ColorSpace : uint8_t { Linear, Srgb };

enum class TextureFilter : uint8_t { Nearest, Linear };
enum class TextureWrap : uint8_t { Repeat, MirroredRepeat, ClampToEdge };

// How a texture is filtered and addressed.
struct Sampler {
    TextureFilter magnification = TextureFilter::Linear;
    TextureFilter minification = TextureFilter::Linear;
    // Filtering between mip levels; nullopt samples the base level only.
    std::optional<TextureFilter> mipmap = TextureFilter::Linear;
    TextureWrap wrap_u = TextureWrap::Repeat;
    TextureWrap wrap_v = TextureWrap::Repeat;

    auto operator==(const Sampler &) const -> bool = default;
};

// An image and how to sample it.  Its colour space is set by the material
// slot that uses it.
struct Texture {
    std::string name;
    std::shared_ptr<const image::Image> image;
    Sampler sampler;
};

// A material's use of a texture, read with texture coordinate set `set`.
struct MaterialTexture {
    std::shared_ptr<const Texture> texture;
    uint8_t set = 0;
};

} // namespace balsa::scene_graph
