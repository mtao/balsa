#include "balsa/scene_graph/MeshInstance.hpp"

#include <algorithm>

namespace balsa::scene_graph {

auto MeshInstance::view(GeometryView view) const
    -> std::shared_ptr<const MeshGeometry> {
    if (!_geometry || !view.value().derive) return nullptr;
    const auto cached = [&] { return std::ranges::find(_views, view, &CachedView::view); };
    if (const auto it = cached();
        it != _views.end() && it->derived && it->version == _geometry->version()) {
        return it->geometry;
    }
    // derive may cache other views, invalidating iterators.
    auto geometry = view.value().derive(*this);
    auto it = cached();
    if (it == _views.end()) it = _views.insert(_views.end(), CachedView{.view = view});
    it->geometry = std::move(geometry);
    it->version = _geometry->version();
    it->derived = true;
    return it->geometry;
}

} // namespace balsa::scene_graph
