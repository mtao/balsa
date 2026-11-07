#pragma once
#include "Accessor.hpp"

#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <type_traits>
#include <variant>

#include <zipper/storage/layout_types.hpp>
#include <zipper/types.hpp>

namespace balsa::visualization::scene_graph::gltf {

namespace detail {
    // mdspan accessor policy reading little-endian T from raw bytes.
    template <typename T>
    struct LittleEndianAccessor {
        using offset_policy = LittleEndianAccessor;
        using element_type = const T;
        using reference = T;
        using data_handle_type = const std::byte *;

        constexpr auto access(data_handle_type p, std::size_t i) const
            -> reference {
            T value;
            std::memcpy(&value, p + i * sizeof(T), sizeof(T));
            if constexpr (std::endian::native == std::endian::big) {
                if constexpr (std::is_floating_point_v<T>) {
                    using Bits = std::uint32_t;
                    static_assert(sizeof(Bits) == sizeof(T));
                    value = std::bit_cast<T>(
                        std::byteswap(std::bit_cast<Bits>(value)));
                } else {
                    value = std::byteswap(value);
                }
            }
            return value;
        }
        constexpr auto offset(data_handle_type p, std::size_t i) const
            -> data_handle_type {
            return p + i * sizeof(T);
        }
    };
} // namespace detail

// An accessor's values as (rows, columns, count): SCALAR is (1, 1, n),
// VECk (k, 1, n), MATk (k, k, n).
template <typename T>
using AccessorSpan = ::zipper::mdspan<const T,
                                      ::zipper::dextents<3>,
                                      ::zipper::storage::layout_stride,
                                      detail::LittleEndianAccessor<T>>;

// One alternative per glTF component type.
using AccessorData = std::variant<AccessorSpan<std::int8_t>,
                                  AccessorSpan<std::uint8_t>,
                                  AccessorSpan<std::int16_t>,
                                  AccessorSpan<std::uint16_t>,
                                  AccessorSpan<std::uint32_t>,
                                  AccessorSpan<float>>;

// Typed, non-owning view of an accessor's values; valid while its
// Document is.
struct AccessorView {
    AccessorData data;
    // glTF accessor.normalized.
    bool normalized = false;

    auto rows() const -> std::size_t { return extent(0); }
    auto columns() const -> std::size_t { return extent(1); }
    auto count() const -> std::size_t { return extent(2); }
    // SCALAR / VECk / MATk.
    auto type() const -> AccessorType;

  private:
    auto extent(std::size_t r) const -> std::size_t {
        return std::visit([r](const auto &span) { return span.extent(r); },
                          data);
    }
};

} // namespace balsa::visualization::scene_graph::gltf
