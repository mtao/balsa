#pragma once
#include "Image.hpp"

#include <type_traits>

#include <zipper/ArrayBase.hxx>

namespace balsa::image {

namespace detail {
    template <typename T>
    constexpr auto component_of() -> std::optional<ComponentType> {
        if constexpr (std::is_same_v<T, uint8_t>) return ComponentType::UInt8;
        else if constexpr (std::is_same_v<T, uint16_t>) return ComponentType::UInt16;
        else if constexpr (std::is_same_v<T, float>) return ComponentType::Float32;
        else return std::nullopt;
    }
} // namespace detail

template <typename T, std::size_t Channels>
auto Image::view() const -> std::optional<View<const T, Channels>> {
    if (detail::component_of<T>() != m_component) return std::nullopt;
    if (Channels != any_channels && Channels != channels()) return std::nullopt;
    return View<const T, Channels>(reinterpret_cast<const T *>(m_pixels.data()),
                                   ExtentsOf<Channels>(m_extents));
}

template <typename T, std::size_t Channels>
auto Image::view() -> std::optional<View<T, Channels>> {
    if (detail::component_of<T>() != m_component) return std::nullopt;
    if (Channels != any_channels && Channels != channels()) return std::nullopt;
    return View<T, Channels>(reinterpret_cast<T *>(m_pixels.data()),
                             ExtentsOf<Channels>(m_extents));
}

template <typename T, std::size_t Channels>
auto Image::array() const -> std::optional<ArrayView<const T, Channels>> {
    return view<T, Channels>().transform(
        [](const auto &pixels) { return ::zipper::ArrayBase(pixels); });
}

template <typename T, std::size_t Channels>
auto Image::array() -> std::optional<ArrayView<T, Channels>> {
    return view<T, Channels>().transform(
        [](const auto &pixels) { return ::zipper::ArrayBase(pixels); });
}

} // namespace balsa::image
