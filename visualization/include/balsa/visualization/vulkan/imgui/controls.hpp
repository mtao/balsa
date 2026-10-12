#pragma once

#include "balsa/visualization/controls/Panel.hpp"

namespace balsa::visualization::vulkan::imgui {

// Draw every applicable registered panel, grouped into a tab bar, into the
// current ImGui window.  Returns true if any value was modified.
auto draw_controls(const controls::Context &ctx) -> bool;

} // namespace balsa::visualization::vulkan::imgui
