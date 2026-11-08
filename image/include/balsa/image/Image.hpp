#pragma once
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <utility>
#include <vector>

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

// Pixels with interleaved channels, rows top to bottom.  What the channels
// hold (layout()) is recorded when known, e.g. by decode(); otherwise they
// are plain data (e.g. a two-channel normal map is not grey + alpha).
// Colour space is not recorded: it depends on how the image is used (e.g. a
// glTF base colour texture is sRGB, a normal map linear).
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

    Image() = default;
    // Zero-filled; throws std::invalid_argument if `layout` does not have
    // extents' channel count.
    Image(Extents extents, ComponentType component, std::optional<Channels> layout = std::nullopt);
    // Zero-filled, with layout's channel count.
    Image(std::size_t height, std::size_t width, Channels layout, ComponentType component);

    [[nodiscard]] auto extents() const -> const Extents & { return m_extents; }
    [[nodiscard]] auto width() const -> std::size_t { return m_extents.extent(1); }
    [[nodiscard]] auto height() const -> std::size_t { return m_extents.extent(0); }
    [[nodiscard]] auto channels() const -> std::size_t { return m_extents.extent(2); }
    [[nodiscard]] auto component() const -> ComponentType { return m_component; }
    // What the channels hold; nullopt if unknown (plain data).
    [[nodiscard]] auto layout() const -> std::optional<Channels> { return m_layout; }
    // Throws std::invalid_argument if `layout` does not have channels()
    // channels.
    auto set_layout(std::optional<Channels> layout) -> void;
    [[nodiscard]] auto empty() const -> bool { return m_pixels.empty(); }

    [[nodiscard]] auto bytes() const -> std::span<const std::byte> { return m_pixels; }
    [[nodiscard]] auto bytes() -> std::span<std::byte> { return m_pixels; }

    // The pixels as T, or nullopt if T does not match component() or
    // Channels does not match channels().  Borrowed: valid while this image
    // is alive and unresized.
    template <typename T, std::size_t Channels = any_channels>
    [[nodiscard]] auto view() const -> std::optional<View<const T, Channels>>;
    template <typename T, std::size_t Channels = any_channels>
    [[nodiscard]] auto view() -> std::optional<View<T, Channels>>;
    template <typename T, std::size_t Channels = any_channels>
    [[nodiscard]] auto array() const -> std::optional<ArrayView<const T, Channels>>;
    template <typename T, std::size_t Channels = any_channels>
    [[nodiscard]] auto array() -> std::optional<ArrayView<T, Channels>>;

  private:
    Extents m_extents{0, 0, 0};
    ComponentType m_component = ComponentType::UInt8;
    std::optional<Channels> m_layout;
    std::vector<std::byte> m_pixels;
};

} // namespace balsa::image

#include "Image.hxx"
