#pragma once

#include "balsa/visualization/shaders/embedded_sources.hpp"

namespace balsa::visualization {

inline auto find_colormap_index(std::string_view name) -> int {
    const auto names = shaders::colormap_names();
    for (std::size_t i = 0; i < names.size(); ++i) {
        if (name == names[i]) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

}// namespace balsa::visualization
