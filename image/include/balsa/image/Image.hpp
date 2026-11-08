#pragma once
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <utility>
#include <variant>

#include <zipper/Array.hpp>
#include <zipper/ArrayBase.hpp>
#include <zipper/types.hpp>

namespace balsa::image {

// The type of each channel value.
enum class ComponentType : uint8_t {
    UInt8,
    UInt16,
    Float32,
};

[[nodiscard]] constexpr auto component_size(ComponentType type) -> std::size_t {
    switch (type) {
    case ComponentType::UInt8: return 1;
    case ComponentType::UInt16: return 2;
    case ComponentType::Float32: return 4;
    }
    return 0;
}

// The C++ types of ComponentType.
template <typename T>
concept Component = std::same_as<T, uint8_t> || std::same_as<T, uint16_t> || std::same_as<T, float>;

template <Component T>
constexpr ComponentType component_type_of = std::same_as<T, uint8_t>    ? ComponentType::UInt8
                                            : std::same_as<T, uint16_t> ? ComponentType::UInt16
                                                                        : ComponentType::Float32;

// Calls f.template operator()<T>() with T the C++ type of `component`.
template <typename F>
auto with_component(ComponentType component, F &&f) -> decltype(auto) {
    switch (component) {
    case ComponentType::UInt16: return f.template operator()<uint16_t>();
    case ComponentType::Float32: return f.template operator()<float>();
    case ComponentType::UInt8: break;
    }
    return f.template operator()<uint8_t>();
}

// What an image's channels hold; the value is the channel count.
enum class Channels : uint8_t {
    Grey = 1,
    GreyAlpha = 2,
    Rgb = 3,
    Rgba = 4,
};

[[nodiscard]] constexpr auto channel_count(Channels layout) -> std::size_t {
    return static_cast<std::size_t>(layout);
}

// Pixel values indexed (row, column, channel), rows top to bottom; row-major,
// so each pixel's channels are contiguous.
template <Component T>
using Pixels = ::zipper::Array<T, ::zipper::dynamic_extent, ::zipper::dynamic_extent,
                               ::zipper::dynamic_extent>;

// An image: pixels of a component type chosen at run time, held as a zipper
// array (pixels<T>(), visit()), plus what the channels hold (layout()) when
// known, e.g. from decode(); otherwise they are plain data (a two-channel
// normal map is not grey + alpha).  Axes 0 and 1 are spatial and axis 2 the
// channels (channel(c) for one channel's plane).  Colour space is not
// recorded: it depends on how the image is used (e.g. a glTF base colour
// texture is sRGB, a normal map linear).
class Image {
  public:
    static constexpr std::size_t any_channels = ::zipper::dynamic_extent;
    // (rows, columns, channels), with a fixed or any channel count.
    template <std::size_t Channels = any_channels>
    using ExtentsOf = ::zipper::extents<::zipper::dynamic_extent, ::zipper::dynamic_extent, Channels>;
    using Extents = ExtentsOf<>;
    // Pixels indexed (row, column, channel).
    template <typename T, std::size_t Channels = any_channels>
    using View = ::zipper::mdspan<T, ExtentsOf<Channels>>;
    // The same pixels as a zipper array, for coefficient-wise operations.
    template <typename T, std::size_t Channels = any_channels>
    using ArrayView = decltype(::zipper::ArrayBase(std::declval<View<T, Channels>>()));
    // One channel's (rows, columns) plane, a view into the pixels.
    template <Component T>
    using ChannelView = decltype(std::declval<const Pixels<T> &>().slice(
        ::zipper::full_extent_t{}, ::zipper::full_extent_t{}, ::zipper::index_type{}));

    Image() = default;
    // Zero-filled; throws std::invalid_argument if `layout` does not have
    // extents' channel count.
    Image(Extents extents, ComponentType component, std::optional<Channels> layout = std::nullopt);
    // Zero-filled, with layout's channel count.
    Image(std::size_t height, std::size_t width, Channels layout, ComponentType component);
    // Takes `pixels`; throws std::invalid_argument as above.
    template <Component T>
    explicit Image(Pixels<T> pixels, std::optional<Channels> layout = std::nullopt);

    [[nodiscard]] auto extents() const -> Extents;
    [[nodiscard]] auto width() const -> std::size_t { return extents().extent(1); }
    [[nodiscard]] auto height() const -> std::size_t { return extents().extent(0); }
    [[nodiscard]] auto channels() const -> std::size_t { return extents().extent(2); }
    [[nodiscard]] auto component() const -> ComponentType {
        return static_cast<ComponentType>(m_pixels.index());
    }
    // What the channels hold; nullopt if unknown (plain data).
    [[nodiscard]] auto layout() const -> std::optional<Channels> { return m_layout; }
    // Throws std::invalid_argument if `layout` does not have channels()
    // channels.
    auto set_layout(std::optional<Channels> layout) -> void;
    [[nodiscard]] auto empty() const -> bool;

    // The pixels, or nullptr if T is not component()'s type.
    template <Component T>
    [[nodiscard]] auto pixels() const -> const Pixels<T> *;
    template <Component T>
    [[nodiscard]] auto pixels() -> Pixels<T> *;
    // Calls f with the pixels (a const Pixels<T> & or Pixels<T> &).
    template <typename F>
    auto visit(F &&f) const -> decltype(auto);
    template <typename F>
    auto visit(F &&f) -> decltype(auto);

    // Channel `c`'s plane, or nullopt if T is not component()'s type.
    template <Component T>
    [[nodiscard]] auto channel(std::size_t c) const -> std::optional<ChannelView<T>>;

    // The pixels' storage, e.g. for GPU upload.
    [[nodiscard]] auto bytes() const -> std::span<const std::byte>;
    [[nodiscard]] auto bytes() -> std::span<std::byte>;

    // The pixels with a fixed channel count, or nullopt if T does not match
    // component() or Channels does not match channels().  Borrowed: valid
    // while this image is alive and unresized.
    template <typename T, std::size_t Channels = any_channels>
    [[nodiscard]] auto view() const -> std::optional<View<const T, Channels>>;
    template <typename T, std::size_t Channels = any_channels>
    [[nodiscard]] auto view() -> std::optional<View<T, Channels>>;
    template <typename T, std::size_t Channels = any_channels>
    [[nodiscard]] auto array() const -> std::optional<ArrayView<const T, Channels>>;
    template <typename T, std::size_t Channels = any_channels>
    [[nodiscard]] auto array() -> std::optional<ArrayView<T, Channels>>;

  private:
    std::variant<Pixels<uint8_t>, Pixels<uint16_t>, Pixels<float>> m_pixels;
    std::optional<Channels> m_layout;
};

} // namespace balsa::image

#include "Image.hxx"
