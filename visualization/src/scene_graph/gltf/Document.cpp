#include "balsa/visualization/scene_graph/gltf/Document.hpp"
#include "balsa/visualization/scene_graph/gltf/detail/Glb.hpp"
#include "balsa/visualization/scene_graph/gltf/detail/LoadErrors.hpp"
#include "balsa/visualization/scene_graph/gltf/detail/accessors.hpp"
#include "balsa/visualization/scene_graph/gltf/detail/meshes.hpp"
#if defined(BALSA_VISUALIZATION_DRACO)
#include "balsa/visualization/scene_graph/gltf/detail/draco.hpp"
#endif
#if defined(BALSA_VISUALIZATION_MESHOPT)
#include "balsa/visualization/scene_graph/gltf/detail/meshopt.hpp"
#endif
#include "balsa/visualization/scene_graph/gltf/detail/nodes.hpp"
#include <balsa/json/reflection.hpp>
#include <balsa/uri/DataUri.hpp>
#include <balsa/uri/Uri.hpp>
#include <balsa/string/capitalization.hpp>

#include <algorithm>
#include <array>
#include <charconv>
#include <format>
#include <fstream>
#include <optional>
#include <ranges>
#include <span>
#include <string_view>

namespace balsa::visualization::scene_graph::gltf {

namespace {

    using detail::LoadErrors;

    using Bytes = std::vector<std::byte>;

    auto error(LoadError::Code code, std::string message)
        -> std::unexpected<LoadError> {
        return std::unexpected(LoadError{code, std::move(message)});
    }

    auto read_file(const std::filesystem::path &path)
        -> std::expected<Bytes, LoadError> {
        std::ifstream ifs(path, std::ios::binary | std::ios::ate);
        if (!ifs) {
            return error(LoadError::Code::Io,
                         std::format("cannot open '{}'", path.string()));
        }
        Bytes bytes(static_cast<std::size_t>(ifs.tellg()));
        ifs.seekg(0);
        if (!ifs.read(reinterpret_cast<char *>(bytes.data()),
                      static_cast<std::streamsize>(bytes.size()))) {
            return error(LoadError::Code::Io,
                         std::format("cannot read '{}'", path.string()));
        }
        return bytes;
    }

    // ── URIs ────────────────────────────────────────────────────────────

    auto to_load_error(uri::UriError e) -> LoadError {
        const auto code = e.code == uri::UriError::Code::Io
                              ? LoadError::Code::Io
                              : LoadError::Code::MissingData;
        return LoadError{code, std::move(e.message)};
    }

    auto uri_error(uri::UriError e) -> std::unexpected<LoadError> {
        return std::unexpected(to_load_error(std::move(e)));
    }

    // Bytes referenced by a buffer or image URI, fetched with `resolver`.
    // Path-relative references are resolved against `base`; absolute URIs
    // go to the resolver as written (so data: is decoded in place, without
    // copying). Other relative forms ("/abs", "//host/x") and absolute
    // file: URIs are rejected so an asset cannot name arbitrary local files.
    auto resolve_uri(std::string_view text,
                     const uri::Uri &base,
                     const uri::Resolver &resolver)
        -> std::expected<Bytes, LoadError> {
        const auto reference = uri::UriView::parse(text);
        if (!reference) { return uri_error(reference.error()); }

        if (!reference->is_relative()) {
            const auto scheme = *reference->scheme();
            if (balsa::string::ascii::IgnoreCaseEqual{}(scheme, "file")) {
                return error(
                    LoadError::Code::MissingData,
                    std::format("absolute file URI '{}' is not allowed", text));
            }
            return resolver.fetch(*reference).transform_error(to_load_error);
        }
        if (!reference->is_path_relative()) {
            return error(
                LoadError::Code::MissingData,
                std::format("reference '{}' is not relative to the asset",
                            text));
        }
        const auto resolved = reference->resolve(base);
        if (!resolved) { return uri_error(resolved.error()); }
        return resolver.fetch(resolved->view()).transform_error(to_load_error);
    }

    // ── Validation ──────────────────────────────────────────────────────

    // "<major>.<minor>"
    // The whole of `text` as an unsigned integer.
    auto parse_unsigned(std::string_view text) -> std::optional<unsigned> {
        unsigned value{};
        const auto *end = text.data() + text.size();
        const auto [ptr, ec] = std::from_chars(text.data(), end, value);
        if (ec != std::errc{} || ptr != end) { return std::nullopt; }
        return value;
    }

    struct Version {
        unsigned major;
        unsigned minor;
    };

    // "<major>.<minor>", as glTF's asset.version / asset.minVersion.
    auto parse_version(std::string_view text) -> std::optional<Version> {
        const auto dot = text.find('.');
        if (dot == text.npos) { return std::nullopt; }
        const auto major = parse_unsigned(text.substr(0, dot));
        const auto minor = parse_unsigned(text.substr(dot + 1));
        if (!major || !minor) { return std::nullopt; }
        return Version{*major, *minor};
    }

    auto check_version(const Asset &asset) -> std::expected<void, LoadError> {
        const auto &text = asset.minVersion.value_or(asset.version);
        const auto version = parse_version(text);
        if (!version) {
            return error(LoadError::Code::InvalidGltf,
                         std::format("malformed version '{}'", text));
        }
        const bool supported =
            version->major == 2 && (!asset.minVersion || version->minor == 0);
        if (!supported) {
            return error(
                LoadError::Code::UnsupportedVersion,
                std::format("glTF version {} (only 2.0 is supported)", text));
        }
        return {};
    }

    // Extensions this build can load.
    constexpr std::array supported_extensions{
        std::string_view("KHR_mesh_quantization"),
#if defined(BALSA_VISUALIZATION_MESHOPT)
        std::string_view("EXT_meshopt_compression"),
        std::string_view("KHR_meshopt_compression"),
#endif
#if defined(BALSA_VISUALIZATION_DRACO)
        std::string_view("KHR_draco_mesh_compression"),
#endif
    };

    auto check_extensions(const glTF &manifest)
        -> std::expected<void, LoadError> {
        const auto unsupported =
            manifest.extensionsRequired
            | std::views::filter([](const std::string &name) {
                  return !std::ranges::contains(supported_extensions, name);
              })
            | std::views::join_with(std::string_view(", "))
            | std::ranges::to<std::string>();
        if (!unsupported.empty()) {
            return error(LoadError::Code::UnsupportedExtension,
                         std::format("unsupported required extensions: {}",
                                     unsupported));
        }
        return {};
    }

    auto parse_manifest(std::span<const std::byte> json)
        -> std::expected<glTF, LoadError> {
        const auto *begin = reinterpret_cast<const char *>(json.data());
        nlohmann::json parsed;
        try {
            parsed = nlohmann::json::parse(begin, begin + json.size());
        } catch (const nlohmann::json::parse_error &e) {
            return error(LoadError::Code::InvalidJson, e.what());
        }
        try {
            return parsed.get<glTF>();
        } catch (const nlohmann::json::exception &e) {
            return error(LoadError::Code::InvalidGltf, e.what());
        } catch (const std::invalid_argument &e) {
            return error(LoadError::Code::InvalidGltf, e.what());
        }
    }

    // The checked manifest of a .gltf / .glb file, plus the GLB binary
    // chunk if any (a view into `file`).
    struct ParsedAsset {
        glTF manifest;
        std::span<const std::byte> glb_bin;
    };

    auto parse_asset(std::span<const std::byte> file)
        -> std::expected<ParsedAsset, LoadError> {
        std::span<const std::byte> json = file;
        std::span<const std::byte> glb_bin;
        if (detail::GlbContents::is_glb(file)) {
            auto contents = detail::GlbContents::parse(file);
            if (!contents) {
                return std::unexpected(std::move(contents.error()));
            }
            json = contents->json.data;
            if (contents->bin) { glb_bin = contents->bin->data; }
        }
        auto manifest = parse_manifest(json);
        if (!manifest) { return std::unexpected(std::move(manifest.error())); }
        if (auto ok = check_version(manifest->asset); !ok) {
            return std::unexpected(std::move(ok.error()));
        }
        if (auto ok = check_extensions(*manifest); !ok) {
            return std::unexpected(std::move(ok.error()));
        }
        return ParsedAsset{std::move(*manifest), glb_bin};
    }

    // Every bufferView's range lies within its buffer's byteLength.
    void check_buffer_views(const glTF &manifest, LoadErrors &errors) {
        for (const auto &[i, view] : std::views::enumerate(manifest.bufferViews)) {
            const auto buffer = static_cast<std::size_t>(view.buffer);
            if (buffer >= manifest.buffers.size()) {
                errors.add(LoadError::Code::MissingData,
                           std::format("bufferView {} refers to missing "
                                       "buffer {}",
                                       i,
                                       buffer));
                continue;
            }
            const auto size =
                static_cast<std::size_t>(manifest.buffers[buffer].byteLength);
            const auto offset = static_cast<std::size_t>(view.byteOffset);
            const auto length = static_cast<std::size_t>(view.byteLength);
            if (offset > size || length > size - offset) {
                errors.add(
                    LoadError::Code::MissingData,
                    std::format("bufferView {} exceeds buffer {}", i, buffer));
            }
        }
    }

} // namespace

auto load_manifest(const std::filesystem::path &path)
    -> std::expected<glTF, LoadError> {
    return read_file(path).and_then(parse_asset).transform(
        [](ParsedAsset asset) { return std::move(asset.manifest); });
}

auto load(const std::filesystem::path &path, const LoadOptions &options)
    -> std::expected<Document, LoadError> {
    // Kept alive for asset->glb_bin, which views into it.
    const auto file = read_file(path);
    auto asset = file.and_then(parse_asset);
    if (!asset) { return std::unexpected(std::move(asset.error())); }

    // The asset's own URI, against which its relative URIs are resolved.
    const auto base = uri::Uri::from_path(path);
    if (!base) { return uri_error(base.error()); }

    Document document(std::move(asset->manifest));
    const auto &manifest = document.m_manifest;
    const auto fetch = [&](std::string_view uri) {
        return resolve_uri(uri, *base, options.resolver);
    };
    // Every problem is collected, so a broken asset reports all of them.
    LoadErrors errors;

    document.m_buffers =
        std::views::enumerate(manifest.buffers)
        | std::views::transform([&](const auto &entry) -> Bytes {
              const auto &[i, buffer] = entry;
              Bytes bytes;
              if (buffer.uri) {
                  auto fetched = fetch(*buffer.uri);
                  if (!fetched) {
                      errors.add(std::format("buffer {}", i),
                                 std::move(fetched.error()));
                      return {};
                  }
                  bytes = std::move(*fetched);
              } else if (i == 0 && !asset->glb_bin.empty()) {
                  // Only the first buffer may refer to the GLB binary chunk.
                  bytes.assign(asset->glb_bin.begin(), asset->glb_bin.end());
#if defined(BALSA_VISUALIZATION_MESHOPT)
              } else if (detail::is_meshopt_fallback(buffer)) {
                  // Only read through compressed views.
                  return {};
#endif
              } else {
                  errors.add(LoadError::Code::MissingData,
                             std::format("buffer {} has no uri and no GLB "
                                         "binary chunk",
                                         i));
                  return {};
              }
              if (bytes.size() < static_cast<std::size_t>(buffer.byteLength)) {
                  errors.add(LoadError::Code::MissingData,
                             std::format("buffer {} has {} bytes, but "
                                         "byteLength is {}",
                                         i,
                                         bytes.size(),
                                         buffer.byteLength));
              }
              return bytes;
          })
        | std::ranges::to<std::vector>();

    check_buffer_views(manifest, errors);
    // Compressed views are decoded into their own range of their buffer.
    std::vector<bool> decoded(manifest.bufferViews.size(), false);
#if defined(BALSA_VISUALIZATION_MESHOPT)
    for (auto &[v, bytes] : detail::decode_meshopt_views(manifest, document.m_buffers, errors)) {
        const auto &view = manifest.bufferViews[v];
        auto &buffer = document.m_buffers[static_cast<std::size_t>(view.buffer)];
        buffer.resize(std::max(buffer.size(),
                               static_cast<std::size_t>(manifest.buffers[view.buffer].byteLength)));
        std::ranges::copy(bytes, buffer.begin() + view.byteOffset);
        decoded[v] = true;
    }
#endif
    // Other views must lie within their buffer's data (fallback buffers have
    // none).
    for (const auto &[i, view] : std::views::enumerate(manifest.bufferViews)) {
        const auto b = static_cast<std::size_t>(view.buffer);
        if (decoded[static_cast<std::size_t>(i)] || b >= document.m_buffers.size()) {
            continue;
        }
        if (static_cast<std::size_t>(view.byteOffset + view.byteLength)
            > document.m_buffers[b].size()) {
            errors.add(LoadError::Code::MissingData,
                       std::format("bufferView {} reads past buffer {}'s data", i, b));
        }
    }

    document.m_uri_images =
        std::views::enumerate(manifest.images)
        | std::views::transform([&](const auto &entry) -> Bytes {
              const auto &[i, image] = entry;
              if (image.uri) {
                  auto fetched = fetch(*image.uri);
                  if (!fetched) {
                      errors.add(std::format("image {}", i),
                                 std::move(fetched.error()));
                      return {};
                  }
                  return std::move(*fetched);
              }
              // bufferView images are viewed in place; just check the index.
              if (!image.bufferView) {
                  errors.add(LoadError::Code::MissingData,
                             std::format("image {} has neither uri nor "
                                         "bufferView",
                                         i));
              } else if (static_cast<std::size_t>(*image.bufferView)
                         >= manifest.bufferViews.size()) {
                  errors.add(LoadError::Code::MissingData,
                             std::format("image {} refers to missing "
                                         "bufferView {}",
                                         i,
                                         *image.bufferView));
              }
              return {};
          })
        | std::ranges::to<std::vector>();

    detail::check_accessors(manifest, errors);
    detail::check_meshes(manifest, errors);
    detail::check_nodes(manifest, errors);
    // Sparse accessors read the buffers, so need everything else valid.
    if (errors.empty()) {
        const auto views =
            std::views::iota(std::size_t{0}, manifest.bufferViews.size())
            | std::views::transform(
                [&](std::size_t v) { return document.buffer_view(v); })
            | std::ranges::to<std::vector>();
        document.m_accessors =
            detail::materialize_accessors(manifest, views, errors);
#if defined(BALSA_VISUALIZATION_DRACO)
        for (auto &&[values, decoded] :
             std::views::zip(document.m_accessors,
                             detail::decode_draco_accessors(manifest, views, errors))) {
            if (decoded) values = std::move(*decoded);
        }
#endif
    }

    if (!errors.empty()) { return std::unexpected(std::move(errors).combined()); }
    return document;
}

auto Document::buffer(std::size_t i) const -> std::span<const std::byte> {
    return std::span(m_buffers[i])
        .first(static_cast<std::size_t>(m_manifest.buffers[i].byteLength));
}

auto Document::buffer_view(std::size_t i) const -> std::span<const std::byte> {
    const auto &view = m_manifest.bufferViews[i];
    return buffer(static_cast<std::size_t>(view.buffer))
        .subspan(static_cast<std::size_t>(view.byteOffset),
                 static_cast<std::size_t>(view.byteLength));
}

auto Document::image(std::size_t i) const -> std::span<const std::byte> {
    const auto &entry = m_manifest.images[i];
    if (entry.uri) { return m_uri_images[i]; }
    return buffer_view(static_cast<std::size_t>(*entry.bufferView));
}

namespace {
    template <typename T>
    auto make_accessor_span(const std::byte *data,
                            const detail::AccessorLayout &layout,
                            std::size_t count,
                            std::size_t stride) -> AccessorSpan<T> {
        using Span = AccessorSpan<T>;
        // mdspan strides are in elements of T, not bytes.
        const typename Span::mapping_type mapping(
            ::zipper::dextents<3>(layout.rows, layout.columns, count),
            std::array<std::size_t, 3>{
                1, layout.column_stride / sizeof(T), stride / sizeof(T)});
        return Span(data, mapping);
    }
} // namespace

auto Document::accessor(std::size_t i) const -> AccessorView {
    const auto &entry = m_manifest.accessors[i];
    const auto layout = detail::accessor_layout(entry);
    const auto count = static_cast<std::size_t>(entry.count);

    const std::byte *data = nullptr;
    std::size_t stride = layout.element_size;
    if (!m_accessors[i].empty()) {
        data = m_accessors[i].data();
    } else {
        const auto view_index = static_cast<std::size_t>(*entry.bufferView);
        data = buffer_view(view_index).data()
               + static_cast<std::size_t>(entry.byteOffset);
        if (const auto byte_stride = m_manifest.bufferViews[view_index].byteStride) {
            stride = static_cast<std::size_t>(*byte_stride);
        }
    }

    const auto make = [&]<typename T>() -> AccessorData {
        return make_accessor_span<T>(data, layout, count, stride);
    };
    AccessorData values = [&] {
        switch (entry.componentType) {
        case ComponentType::BYTE: return make.template operator()<std::int8_t>();
        case ComponentType::UNSIGNED_BYTE:
            return make.template operator()<std::uint8_t>();
        case ComponentType::SHORT: return make.template operator()<std::int16_t>();
        case ComponentType::UNSIGNED_SHORT:
            return make.template operator()<std::uint16_t>();
        case ComponentType::UNSIGNED_INT:
            return make.template operator()<std::uint32_t>();
        case ComponentType::FLOAT: break;
        }
        return make.template operator()<float>();
    }();
    return AccessorView{.data = std::move(values), .normalized = entry.normalized};
}

} // namespace balsa::visualization::scene_graph::gltf
