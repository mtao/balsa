#pragma once
#include <algorithm>
#include <array>
#include <bit>
#include <cassert>
#include <cstddef>
#include <cstring>
#include <meta>
#include <optional>
#include <span>
#include <type_traits>

namespace balsa::visualization::gltf::detail {

// Little-endian, padding-free POD at the start of `bytes`; nullopt if
// `bytes` is too short.
template <typename T>
    requires std::is_trivially_copyable_v<T>
             && std::has_unique_object_representations_v<T>
auto read_little_endian(std::span<const std::byte> bytes) -> std::optional<T> {
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

// Writes `value` little-endian at the start of `bytes`.
template <typename T>
    requires std::is_arithmetic_v<T>
void write_little_endian(std::span<std::byte> bytes, T value) {
    assert(bytes.size() >= sizeof(T));
    auto representation = std::bit_cast<std::array<std::byte, sizeof(T)>>(value);
    if constexpr (std::endian::native == std::endian::big) {
        std::ranges::reverse(representation);
    }
    std::ranges::copy(representation, bytes.begin());
}

} // namespace balsa::visualization::gltf::detail
