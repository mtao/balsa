#include "balsa/image/Image.hpp"

namespace balsa::image {

Image::Image(Extents extents, ComponentType component)
  : m_extents(extents),
    m_component(component),
    m_pixels(extents.extent(0) * extents.extent(1) * extents.extent(2)
             * component_size(component)) {}

} // namespace balsa::image
