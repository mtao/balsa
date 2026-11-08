#pragma once
#include "Image.hpp"

#include <type_traits>

#include <zipper/ArrayBase.hxx>

namespace balsa::image {

template <Component T>
Image::Image(Pixels<T> pixels, std::optional<Channels> layout) : m_pixels(std::move(pixels)) {
    set_layout(layout);
}

template <Component T>
auto Image::pixels() const -> const Pixels<T> * {
    return std::get_if<Pixels<T>>(&m_pixels);
}

template <Component T>
auto Image::pixels() -> Pixels<T> * {
    return std::get_if<Pixels<T>>(&m_pixels);
}

template <typename F>
auto Image::visit(F &&f) const -> decltype(auto) {
    return std::visit(std::forward<F>(f), m_pixels);
}

template <typename F>
auto Image::visit(F &&f) -> decltype(auto) {
    return std::visit(std::forward<F>(f), m_pixels);
}

template <Component T>
auto Image::channel(std::size_t c) const -> std::optional<ChannelView<T>> {
    const auto *p = pixels<T>();
    if (p == nullptr) return std::nullopt;
    return p->slice(::zipper::full_extent_t{}, ::zipper::full_extent_t{},
                    static_cast<::zipper::index_type>(c));
}

template <typename T, std::size_t Channels>
auto Image::view() const -> std::optional<View<const T, Channels>> {
    if constexpr (!Component<T>) {
        return std::nullopt;
    } else {
        const auto *p = pixels<T>();
        if (p == nullptr) return std::nullopt;
        if (Channels != any_channels && Channels != channels()) return std::nullopt;
        return View<const T, Channels>(p->expression().data(), ExtentsOf<Channels>(extents()));
    }
}

template <typename T, std::size_t Channels>
auto Image::view() -> std::optional<View<T, Channels>> {
    if constexpr (!Component<T>) {
        return std::nullopt;
    } else {
        auto *p = pixels<T>();
        if (p == nullptr) return std::nullopt;
        if (Channels != any_channels && Channels != channels()) return std::nullopt;
        return View<T, Channels>(p->expression().data(), ExtentsOf<Channels>(extents()));
    }
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
