#pragma once
#include <bit>
#include <cstring>
#include <meta>
#include <optional>
#include <span>
#include <type_traits>

namespace balsa::visualization::scene_graph::gltf::detail {

// Little-endian, padding-free POD at the start of `bytes`; nullopt if
// `bytes` is too short.
template <typename T>
    requires std::is_trivially_copyable_v<T>
             && std::has_unique_object_representations_v<T>
auto read_le(std::span<const std::byte> bytes) -> std::optional<T> {
    if (bytes.size() < sizeof(T)) { return std::nullopt; }
    T value;
    std::memcpy(&value, bytes.data(), sizeof(T));
    if constexpr (std::endian::native == std::endian::little) {
    } else if constexpr (std::is_integral_v<T>) {
        value = std::byteswap(value);
    } else {
        constexpr auto ctx = std::meta::access_context::unchecked();
        template for (constexpr auto m : std::define_static_array(
                          std::meta::nonstatic_data_members_of(^^T, ctx))) {
            value.[:m:] = std::byteswap(value.[:m:]);
        }
    }
    return value;
}

} // namespace balsa::visualization::scene_graph::gltf::detail
