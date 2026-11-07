#include "balsa/scene_graph/GeometryView.hpp"

#include "balsa/scene_graph/MeshGeometry.hpp"
#include "balsa/scene_graph/MeshInstance.hpp"

#include <balsa/geometry/simplicial_complex/boundary.hpp>
#include <balsa/geometry/simplicial_complex/faces.hpp>
#include <zipper/MatrixBase.hxx>

namespace balsa::scene_graph {

namespace {
    namespace sc = balsa::geometry::simplicial_complex;
    using Geometry = std::shared_ptr<const MeshGeometry>;

    // `source`'s vertex attributes with these D-simplices; null if there are
    // none (e.g. a closed surface's boundary).
    template <std::size_t D, typename Simplices>
    auto with_simplices(const MeshGeometry &source, const Simplices &simplices)
        -> Geometry {
        if (simplices.extent(1) == 0) return nullptr;
        auto view = std::make_shared<MeshGeometry>(source.share_vertex_attributes());
        view->set_simplices<D>(simplices.expression().as_mdspan());
        return view;
    }

    auto boundary_of(const MeshGeometry &g) -> Geometry {
        switch (g.dimension()) {
        case 3:
            return with_simplices<2>(g, sc::boundary(::zipper::MatrixBase(g.simplices<3>())));
        case 2:
            return with_simplices<1>(g, sc::boundary(::zipper::MatrixBase(g.simplices<2>())));
        case 1:
            return with_simplices<0>(g, sc::boundary(::zipper::MatrixBase(g.simplices<1>())));
        default:
            return nullptr;
        }
    }

    // Stored edges, else all edges of the top dimension.
    auto edges_of(const Geometry &geometry) -> Geometry {
        const auto &g = *geometry;
        if (g.dimension() == 1) return geometry;
        if (g.has_simplices(1)) {
            // E.g. a polygon mesh's edges, without triangulation diagonals.
            auto view = std::make_shared<MeshGeometry>(g.share_vertex_attributes());
            view->set_simplices<1>(g.simplices<1>());
            return view;
        }
        switch (g.dimension()) {
        case 3: return with_simplices<1>(g, sc::faces<2>(::zipper::MatrixBase(g.simplices<3>())));
        case 2: return with_simplices<1>(g, sc::faces<2>(::zipper::MatrixBase(g.simplices<2>())));
        default: return nullptr;
        }
    }

    // Built-ins are added in declaration order.
    struct Defaults {
        catalog::Catalog<GeometryViewDefinition> catalog{"geometry view"};
        GeometryView source = catalog.add(
            {"balsa.source", "Source", [](const MeshInstance &instance) -> Geometry {
                 return instance.shared_geometry();
             }});
        GeometryView boundary = catalog.add(
            {"balsa.boundary", "Boundary", [](const MeshInstance &instance) {
                 return boundary_of(*instance.geometry());
             }});
        GeometryView edges = catalog.add(
            {"balsa.edges", "Edges", [](const MeshInstance &instance) {
                 return edges_of(instance.shared_geometry());
             }});
        GeometryView boundary_edges = catalog.add(
            {"balsa.boundary_edges", "Boundary edges",
             [this](const MeshInstance &instance) -> Geometry {
                 const auto faces = instance.view(boundary);
                 return faces ? edges_of(faces) : nullptr;
             }});
    };

    auto defaults() -> Defaults & {
        static Defaults d;
        return d;
    }
} // namespace

auto geometry_views() -> catalog::Catalog<GeometryViewDefinition> & {
    return defaults().catalog;
}

namespace views {
    auto source() -> GeometryView { return defaults().source; }
    auto boundary() -> GeometryView { return defaults().boundary; }
    auto edges() -> GeometryView { return defaults().edges; }
    auto boundary_edges() -> GeometryView { return defaults().boundary_edges; }

    auto surface_for(std::size_t dimension) -> GeometryView {
        return dimension == 3 ? boundary() : source();
    }
    auto wireframe_for(std::size_t dimension) -> GeometryView {
        return dimension == 3 ? boundary_edges() : edges();
    }
} // namespace views

} // namespace balsa::scene_graph
