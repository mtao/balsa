#include "balsa/visualization/scene_graph/gltf/detail/meshopt.hpp"
#include "balsa/visualization/scene_graph/gltf/detail/extension.hpp"

#include <meshoptimizer.h>

#include <array>
#include <format>
#include <ranges>
#include <string>

namespace balsa::visualization::gltf::detail {

namespace {
    enum class Mode { ATTRIBUTES, TRIANGLES, INDICES };
    enum class Filter { NONE, OCTAHEDRAL, QUATERNION, EXPONENTIAL, COLOR };

    auto from_json(const nlohmann::json &j, Mode &v) -> void { json::enum_from_json(j, v); }
    auto from_json(const nlohmann::json &j, Filter &v) -> void { json::enum_from_json(j, v); }

    // A bufferView's meshopt extension: where its compressed data lies.
    struct [[=object]] MeshoptView {
        [[=json::required]] std::size_t buffer;
        [[=json::skip_default]] std::size_t byteOffset = 0;
        [[=json::required]] std::size_t byteLength;
        [[=json::required]] std::size_t byteStride;
        [[=json::required]] std::size_t count;
        [[=json::required]] Mode mode;
        [[=json::skip_default]] Filter filter = Filter::NONE;
    };
    auto from_json(const nlohmann::json &j, MeshoptView &v) -> void {
        json::from_json_members(j, v);
    }

    // A buffer's meshopt extension.
    struct [[=object]] MeshoptBuffer {
        bool fallback = false;
    };
    auto from_json(const nlohmann::json &j, MeshoptBuffer &v) -> void {
        json::from_json_members(j, v);
    }

    // The draft and ratified names of the extension.
    constexpr std::array<std::string_view, 2> names{"EXT_meshopt_compression",
                                                    "KHR_meshopt_compression"};

    // Decodes the view described by `ext` into `out`; an error message on
    // failure.
    auto decode(const MeshoptView &ext,
                std::span<const std::vector<std::byte>> buffers,
                std::vector<std::byte> &out) -> std::optional<std::string> {
        const auto [buffer, offset, length, stride, count, mode, filter] = ext;
        if (buffer >= buffers.size() || offset > buffers[buffer].size()
            || length > buffers[buffer].size() - offset) {
            return "compressed data lies outside its buffer";
        }
        const auto *source =
            reinterpret_cast<const unsigned char *>(buffers[buffer].data() + offset);

        out.resize(count * stride);
        int result = -1;
        switch (mode) {
        case Mode::ATTRIBUTES:
            result = meshopt_decodeVertexBuffer(out.data(), count, stride, source, length);
            break;
        case Mode::TRIANGLES:
            result = meshopt_decodeIndexBuffer(out.data(), count, stride, source, length);
            break;
        case Mode::INDICES:
            result = meshopt_decodeIndexSequence(out.data(), count, stride, source, length);
            break;
        }
        if (result != 0) {
            return std::format("{} data is corrupt",
                               mode == Mode::ATTRIBUTES  ? "ATTRIBUTES"
                               : mode == Mode::TRIANGLES ? "TRIANGLES"
                                                             : "INDICES");
        }

        switch (filter) {
        case Filter::NONE: break;
        case Filter::OCTAHEDRAL: meshopt_decodeFilterOct(out.data(), count, stride); break;
        case Filter::QUATERNION: meshopt_decodeFilterQuat(out.data(), count, stride); break;
        case Filter::EXPONENTIAL: meshopt_decodeFilterExp(out.data(), count, stride); break;
        case Filter::COLOR: meshopt_decodeFilterColor(out.data(), count, stride); break;
        }
        return std::nullopt;
    }
} // namespace

auto is_meshopt_fallback(const Buffer &buffer) -> bool {
    const auto ext = parse_extension<MeshoptBuffer>(buffer.extensions, names);
    return ext && *ext && (*ext)->fallback;
}

auto decode_meshopt_views(const glTF &manifest,
                          std::span<const std::vector<std::byte>> buffers,
                          LoadErrors &errors) -> std::vector<DecodedView> {
    std::vector<DecodedView> decoded;
    for (const auto &[i, view] : std::views::enumerate(manifest.bufferViews)) {
        const auto fail = [&](std::string_view message) {
            errors.add(LoadError::Code::InvalidGltf,
                       std::format("bufferView {}: {}", i, message));
        };
        const auto ext = parse_extension<MeshoptView>(view.extensions, names);
        if (!ext) {
            fail(ext.error());
            continue;
        }
        if (!*ext) continue;

        std::vector<std::byte> bytes;
        auto problem = decode(**ext, buffers, bytes);
        if (!problem && bytes.size() != static_cast<std::size_t>(view.byteLength)) {
            problem = std::format("decodes to {} bytes, but byteLength is {}",
                                  bytes.size(), view.byteLength);
        }
        if (problem) {
            fail(*problem);
            continue;
        }
        decoded.push_back({static_cast<std::size_t>(i), std::move(bytes)});
    }
    return decoded;
}

} // namespace balsa::visualization::gltf::detail
