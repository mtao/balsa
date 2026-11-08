#include "balsa/image/Image.hpp"

#include <stdexcept>

namespace balsa::image {

Image::Image(Extents extents, ComponentType component, std::optional<Channels> layout)
  : m_pixels(with_component(component, [&]<Component T>() -> decltype(m_pixels) {
        return Pixels<T>(extents.extent(0), extents.extent(1), extents.extent(2));
    })) {
    set_layout(layout);
}

Image::Image(std::size_t height, std::size_t width, Channels layout, ComponentType component)
  : Image(Extents(height, width, channel_count(layout)), component, layout) {}

auto Image::extents() const -> Extents {
    return visit([](const auto &p) { return Extents(p.extents()); });
}

auto Image::set_layout(std::optional<Channels> layout) -> void {
    if (layout && channel_count(*layout) != channels()) {
        throw std::invalid_argument("image::Image: layout does not match the channel count");
    }
    m_layout = layout;
}

auto Image::empty() const -> bool {
    const auto e = extents();
    return e.extent(0) * e.extent(1) * e.extent(2) == 0;
}

auto Image::bytes() const -> std::span<const std::byte> {
    return visit([&](const auto &p) {
        return std::as_bytes(std::span(p.expression().data(),
                                       height() * width() * channels()));
    });
}

auto Image::bytes() -> std::span<std::byte> {
    const auto count = height() * width() * channels();
    return visit([&](auto &p) {
        return std::as_writable_bytes(std::span(p.expression().data(), count));
    });
}

} // namespace balsa::image
