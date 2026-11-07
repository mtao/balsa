#pragma once
#include <cstddef>
#include <functional>
#include <memory>
#include <string>

#include <balsa/catalog/Catalog.hpp>

namespace balsa::scene_graph {

class MeshGeometry;
class MeshInstance;

// A drawable geometry derived from a MeshInstance's geometry (its boundary,
// its edges, ...).  Cached by the MeshInstance and chosen per render layer.
// Extensible via geometry_views().
struct GeometryViewDefinition {
    std::string id;
    std::string label;
    // nullptr if the view is empty.  May return the geometry itself or use
    // other views via instance.view().  instance.geometry() is non-null.
    std::function<std::shared_ptr<const MeshGeometry>(const MeshInstance &instance)>
        derive;
};

using GeometryView = catalog::Entry<GeometryViewDefinition>;

// Default catalog, with the built-ins below.
auto geometry_views() -> catalog::Catalog<GeometryViewDefinition> &;

// Built-in views.
namespace views {
    // The geometry itself.
    auto source() -> GeometryView;
    // Codimension-1 boundary: a tet mesh's outward boundary triangles, a
    // triangle mesh's border edges; empty below dimension 2.
    auto boundary() -> GeometryView;
    // Stored edges if any, else all edges of the top dimension.
    auto edges() -> GeometryView;
    // Edges of the boundary.
    auto boundary_edges() -> GeometryView;

    // Default surface / wireframe views for a mesh of `dimension`.
    auto surface_for(std::size_t dimension) -> GeometryView;
    auto wireframe_for(std::size_t dimension) -> GeometryView;
} // namespace views

} // namespace balsa::scene_graph
