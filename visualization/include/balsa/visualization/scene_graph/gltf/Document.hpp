#pragma once
#include "AccessorView.hpp"
#include "LoadError.hpp"
#include "glTF.hpp"

#include <balsa/uri/Resolver.hpp>

#include <cstddef>
#include <expected>
#include <filesystem>
#include <span>
#include <vector>

namespace balsa::visualization::scene_graph::gltf {

struct LoadOptions {
    // Fetches the asset's buffer and image URIs. The default handles data:
    // URIs and files relative to the asset; add schemes with add().
    uri::Resolver resolver = uri::Resolver::defaults();
};

class Document;

// Loads a .gltf (JSON) or .glb (binary container) file with all the buffers
// and images it references. The format is detected from the file contents.
// `options` is only used during the call.
auto load(const std::filesystem::path &path, const LoadOptions &options = {})
    -> std::expected<Document, LoadError>;

// Parses and checks only the JSON (of a .gltf, or the JSON chunk of a .glb);
// fetches no buffers or images.
auto load_manifest(const std::filesystem::path &path)
    -> std::expected<glTF, LoadError>;

// A loaded glTF asset: the parsed JSON plus the bytes of every buffer and
// image it references. Only load() creates one, after checking every buffer
// length, bufferView range and image source, so the accessors cannot fail;
// indices must be in range of the corresponding manifest() array.
// Images are not decoded.
class Document {
  public:
    // The parsed JSON (the spec's root glTF object).
    auto manifest() const -> const glTF & { return m_manifest; }

    // manifest().buffers[i].byteLength bytes, from a .bin file, a data: URI
    // or the GLB binary chunk.
    auto buffer(std::size_t i) const -> std::span<const std::byte>;
    // The range of its buffer that manifest().bufferViews[i] covers.
    auto buffer_view(std::size_t i) const -> std::span<const std::byte>;
    // Encoded bytes (e.g. PNG or JPEG) of manifest().images[i], from its URI
    // or bufferView. The media type is in images[i].mimeType (required for
    // bufferView images) or implied by the URI.
    auto image(std::size_t i) const -> std::span<const std::byte>;
    // Typed view of manifest().accessors[i].
    auto accessor(std::size_t i) const -> AccessorView;

  private:
    friend auto load(const std::filesystem::path &, const LoadOptions &)
        -> std::expected<Document, LoadError>;

    explicit Document(glTF manifest) : m_manifest(std::move(manifest)) {}

    glTF m_manifest;
    // Indexed like manifest().buffers.
    std::vector<std::vector<std::byte>> m_buffers;
    // Indexed like manifest().images; empty for bufferView images.
    std::vector<std::vector<std::byte>> m_uri_images;
    // Values of accessors that can't be viewed in place; indexed like
    // manifest().accessors.
    std::vector<std::vector<std::byte>> m_accessors;
};

} // namespace balsa::visualization::scene_graph::gltf
