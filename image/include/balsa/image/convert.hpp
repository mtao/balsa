#pragma once
#include "Image.hpp"

namespace balsa::image {

// `image` rearranged into `layout`, keeping its component type.  Grey
// expands to RGB, and RGB reduces to grey by Rec. 709 luminance; a missing
// alpha is opaque.  Throws std::invalid_argument if the image's layout is
// unknown.
[[nodiscard]] auto convert_channels(const Image &image, Channels layout) -> Image;

// `image` with components of type `component`, keeping its channels.
// Values keep their normalised meaning (e.g. 8-bit 255 becomes 16-bit
// 65535 or float 1); floats outside [0, 1] are clamped when converted to
// integers.
[[nodiscard]] auto convert_component(const Image &image, ComponentType component) -> Image;

} // namespace balsa::image
