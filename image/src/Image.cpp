#include "balsa/image/Image.hpp"

#include <stdexcept>

namespace balsa::image {

Image::Image(Extents extents, ComponentType component, std::optional<Channels> layout)
  : m_extents(extents),
    m_component(component),
    m_pixels(extents.extent(0) * extents.extent(1) * extents.extent(2)
             * component_size(component)) {
    set_layout(layout);
}

Image::Image(std::size_t height, std::size_t width, Channels layout, ComponentType component)
  : Image(Extents(height, width, channel_count(layout)), component, layout) {}

auto Image::set_layout(std::optional<Channels> layout) -> void {
    if (layout && channel_count(*layout) != channels()) {
        throw std::invalid_argument("image::Image: layout does not match the channel count");
    }
    m_layout = layout;
}

} // namespace balsa::image
