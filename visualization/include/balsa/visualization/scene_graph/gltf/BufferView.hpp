#pragma once
#include "Extension.hpp"
#include <cstdint>
#include <optional>
#include <string>

namespace balsa::visualization::scene_graph::gltf {

// The hint representing the intended GPU buffer type to use with this buffer
// view.
enum class BufferViewTarget : int64_t {
    ARRAY_BUFFER = 34962,
    ELEMENT_ARRAY_BUFFER = 34963,
};

// A view into a buffer generally representing a subset of the buffer.
struct [[=object]] BufferView {
    // The index of the buffer.
    [[=json::required]] int64_t buffer;
    // The offset into the buffer in bytes.
    [[=json::skip_default]] int64_t byteOffset = 0;
    // The length of the bufferView in bytes.
    [[=json::required]] int64_t byteLength;
    // The stride, in bytes.
    std::optional<int64_t> byteStride;
    // The hint representing the intended GPU buffer type to use with this
    // buffer view.
    std::optional<BufferViewTarget> target;
    // The user-defined name of this object.
    std::optional<std::string> name;
    // JSON object with extension-specific objects.
    Extension extensions;
    // Application-specific data.
    Extras extras;
};

auto to_json(nlohmann::json &j, const BufferView &v) -> void;
auto from_json(const nlohmann::json &j, BufferView &v) -> void;

} // namespace balsa::visualization::scene_graph::gltf
