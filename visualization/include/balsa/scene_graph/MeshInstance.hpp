#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "AbstractFeature.hpp"
#include "GeometryView.hpp"
#include "MeshGeometry.hpp"

namespace balsa::scene_graph {

// ── MeshInstance ────────────────────────────────────────────────────
//
// Feature saying which MeshGeometry an Object shows.  The geometry is a
// shared resource: several Objects can show the same one (each with its
// own shading), and it outlives any of them.  Shading features on the
// Object (e.g. BasicShading) read the geometry from here.
//
// Also caches its GeometryViews, rebuilt when the geometry changes.

class MeshInstance : public AbstractFeature {
  public:
    MeshInstance() = default;
    explicit MeshInstance(std::shared_ptr<MeshGeometry> geometry)
      : _geometry(std::move(geometry)) {}

    /// The shown geometry, or nullptr.
    [[nodiscard]] auto geometry() -> MeshGeometry * { return _geometry.get(); }
    [[nodiscard]] auto geometry() const -> const MeshGeometry * {
        return _geometry.get();
    }
    [[nodiscard]] auto shared_geometry() const
        -> const std::shared_ptr<MeshGeometry> & {
        return _geometry;
    }
    auto set_geometry(std::shared_ptr<MeshGeometry> geometry) -> void {
        _geometry = std::move(geometry);
        _views.clear();
    }

    /// The cached view; nullptr without a geometry or if the view is empty.
    [[nodiscard]] auto view(GeometryView view) const
        -> std::shared_ptr<const MeshGeometry>;

  private:
    struct CachedView {
        GeometryView view;
        bool derived = false;
        uint64_t version = 0;
        std::shared_ptr<const MeshGeometry> geometry{};
    };

    std::shared_ptr<MeshGeometry> _geometry;
    mutable std::vector<CachedView> _views;
};

} // namespace balsa::scene_graph
