#pragma once
#include <memory>
#include <utility>

#include "AbstractFeature.hpp"
#include "MeshGeometry.hpp"

namespace balsa::scene_graph {

// ── MeshInstance ────────────────────────────────────────────────────
//
// Feature saying which MeshGeometry an Object shows.  The geometry is a
// shared resource: several Objects can show the same one (each with its
// own shading), and it outlives any of them.  Shading features on the
// Object (e.g. BasicShading) read the geometry from here.

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
    }

  private:
    std::shared_ptr<MeshGeometry> _geometry;
};

} // namespace balsa::scene_graph
