#include "balsa/visualization/scene_graph/gltf/AccessorView.hpp"

namespace balsa::visualization::gltf {

auto AccessorView::type() const -> AccessorType {
    if (columns() == 1) {
        switch (rows()) {
        case 1: return AccessorType::SCALAR;
        case 2: return AccessorType::VEC2;
        case 3: return AccessorType::VEC3;
        default: return AccessorType::VEC4;
        }
    }
    switch (rows()) {
    case 2: return AccessorType::MAT2;
    case 3: return AccessorType::MAT3;
    default: return AccessorType::MAT4;
    }
}

} // namespace balsa::visualization::gltf
