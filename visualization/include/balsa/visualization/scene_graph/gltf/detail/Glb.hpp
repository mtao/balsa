#pragma once
// GLB binary container: a 12-byte header followed by chunks.
// https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#glb-file-format-specification
#include "../LoadError.hpp"

#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <vector>

namespace balsa::visualization::gltf::detail {

struct GLBHeader {
    static constexpr std::uint32_t glTF = 0x46546C67; // "glTF"

    uint32_t magic;
    uint32_t version;
    uint32_t length;
};

struct GLBChunkHeader {
    static constexpr std::uint32_t JSON = 0x4E4F534A; // "JSON"
    static constexpr std::uint32_t BIN = 0x004E4942; // "BIN\0"

    uint32_t chunkLength;
    uint32_t chunkType;
};

struct GlbChunk {
    GLBChunkHeader header;
    std::span<const std::byte> data;
};

// A parsed GLB container. Chunk data points into the bytes passed to
// parse(), which must outlive this object.
struct GlbContents {
    GLBHeader header;
    // The first chunk; always JSON.
    GlbChunk json;
    // The first BIN chunk, if any.
    std::optional<GlbChunk> bin;
    // All remaining chunks in file order (e.g. extension chunks).
    std::vector<GlbChunk> extra;

    // True if this has a valid glb header
    static auto is_glb(std::span<const std::byte> bytes) -> bool;

    static auto parse(std::span<const std::byte> bytes)
        -> std::expected<GlbContents, LoadError>;
};

} // namespace balsa::visualization::gltf::detail
