#include "balsa/visualization/scene_graph/gltf/Document.hpp"
#include "balsa/visualization/scene_graph/gltf/detail/Glb.hpp"
#include <balsa/json/reflection.hpp>
#include <balsa/uri/DataUri.hpp>
#include <balsa/uri/Uri.hpp>

#include <algorithm>
#include <cctype>
#include <charconv>
#include <format>
#include <fstream>
#include <optional>
#include <ranges>
#include <span>
#include <string_view>

namespace balsa::visualization::scene_graph::gltf {

namespace {

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
            if (scheme.size() == 4
                && std::ranges::equal(
                    scheme, std::string_view("file"), [](char a, char b) {
                        return std::tolower(static_cast<unsigned char>(a)) == b;
                    })) {
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

    auto check_extensions(const glTF &manifest)
        -> std::expected<void, LoadError> {
        // No extensions are supported yet.
        if (!manifest.extensionsRequired.empty()) {
            const auto names = manifest.extensionsRequired
                               | std::views::join_with(std::string_view(", "))
                               | std::ranges::to<std::string>();
            return error(
                LoadError::Code::UnsupportedExtension,
                std::format("unsupported required extensions: {}", names));
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

    // Problems found while loading, reported together as one LoadError.
    class LoadErrors {
      public:
        void add(LoadError::Code code, std::string message) {
            m_errors.push_back(LoadError{code, std::move(message)});
        }
        // `error`, with its message prefixed by what it was loading.
        void add(std::string_view context, LoadError error) {
            error.message = std::format("{}: {}", context, error.message);
            m_errors.push_back(std::move(error));
        }

        auto empty() const -> bool { return m_errors.empty(); }

        // The first error's code, with every message joined.
        auto combined() && -> LoadError {
            if (m_errors.size() == 1) { return std::move(m_errors.front()); }
            auto messages =
                m_errors
                | std::views::transform(&LoadError::message)
                | std::views::join_with(std::string_view("; "))
                | std::ranges::to<std::string>();
            return LoadError{
                m_errors.front().code,
                std::format("{} errors: {}", m_errors.size(), messages)};
        }

      private:
        std::vector<LoadError> m_errors;
    };

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

} // namespace balsa::visualization::scene_graph::gltf
