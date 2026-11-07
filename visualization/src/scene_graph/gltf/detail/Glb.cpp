#include "balsa/visualization/scene_graph/gltf/detail/Glb.hpp"

#include "balsa/visualization/scene_graph/gltf/detail/little_endian.hpp"

#include <format>

namespace balsa::visualization::scene_graph::gltf::detail {

namespace {
    auto error(LoadError::Code code, std::string message)
        -> std::unexpected<LoadError> {
        return std::unexpected(LoadError{code, std::move(message)});
    }
} // namespace

auto GlbContents::is_glb(std::span<const std::byte> bytes) -> bool {
    return read_little_endian<std::uint32_t>(bytes) == GLBHeader::glTF;
}

auto GlbContents::parse(std::span<const std::byte> bytes)
    -> std::expected<GlbContents, LoadError> {
    const auto header = read_little_endian<GLBHeader>(bytes);
    if (!header) {
        return error(LoadError::Code::InvalidGlb, "truncated GLB header");
    }
    if (header->version != 2) {
        return error(
            LoadError::Code::UnsupportedVersion,
            std::format("GLB container version {} (only 2 is supported)",
                        header->version));
    }
    if (header->length > bytes.size()) {
        return error(LoadError::Code::InvalidGlb,
                     "GLB length exceeds the file size");
    }
    if (header->length < sizeof(GLBHeader)) {
        return error(LoadError::Code::InvalidGlb,
                     "GLB length is shorter than its header");
    }
    auto rest = bytes.first(header->length).subspan(sizeof(GLBHeader));

    std::optional<GlbChunk> json;
    std::optional<GlbChunk> bin;
    std::vector<GlbChunk> extra;
    while (!rest.empty()) {
        const auto chunk_header = read_little_endian<GLBChunkHeader>(rest);
        if (!chunk_header) {
            return error(LoadError::Code::InvalidGlb,
                         "truncated GLB chunk header");
        }
        rest = rest.subspan(sizeof(GLBChunkHeader));
        if (rest.size() < chunk_header->chunkLength) {
            return error(LoadError::Code::InvalidGlb,
                         "GLB chunk exceeds the file size");
        }
        const GlbChunk chunk{*chunk_header,
                             rest.first(chunk_header->chunkLength)};
        rest = rest.subspan(chunk_header->chunkLength);

        if (!json) {
            if (chunk.header.chunkType != GLBChunkHeader::JSON) {
                return error(LoadError::Code::InvalidGlb,
                             "first GLB chunk is not JSON");
            }
            json = chunk;
        } else if (chunk.header.chunkType == GLBChunkHeader::BIN && !bin) {
            bin = chunk;
        } else {
            extra.push_back(chunk);
        }
    }
    if (!json) {
        return error(LoadError::Code::InvalidGlb, "GLB has no JSON chunk");
    }
    return GlbContents{*header, *json, bin, std::move(extra)};
}

} // namespace balsa::visualization::scene_graph::gltf::detail
