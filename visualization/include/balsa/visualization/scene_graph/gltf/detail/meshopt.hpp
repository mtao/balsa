#pragma once
#include "../glTF.hpp"
#include "LoadErrors.hpp"

#include <cstddef>
#include <span>
#include <vector>

namespace balsa::visualization::gltf::detail {

// Whether `buffer` is a meshopt fallback buffer, whose data may be absent.
auto is_meshopt_fallback(const Buffer &buffer) -> bool;

// A compressed buffer view's decoded bytes, which belong at the view's own
// range of its (fallback) buffer.
struct DecodedView {
    std::size_t view;
    std::vector<std::byte> bytes;
};

// Decodes the meshopt-compressed buffer views.  `buffers` holds the
// buffers' data.
auto decode_meshopt_views(const glTF &manifest,
                          std::span<const std::vector<std::byte>> buffers,
                          LoadErrors &errors) -> std::vector<DecodedView>;

} // namespace balsa::visualization::gltf::detail
