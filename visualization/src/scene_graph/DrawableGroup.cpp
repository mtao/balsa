#include "balsa/scene_graph/DrawableGroup.hpp"
#include "balsa/scene_graph/Drawable.hpp"

#include <algorithm>

namespace balsa::scene_graph {

void DrawableGroup::add(Drawable &d) {
    _drawables.push_back(&d);
}

void DrawableGroup::remove(Drawable &d) {
    auto it = std::find(_drawables.begin(), _drawables.end(), &d);
    if (it != _drawables.end()) {
        _drawables.erase(it);
    }
}

}// namespace balsa::scene_graph
