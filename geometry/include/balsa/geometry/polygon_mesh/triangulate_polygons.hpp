#if !defined(BALSA_GEOMETRY_POLYGON_MESH_TRIANGULATE_POLYGONS_HPP)
#define BALSA_GEOMETRY_POLYGON_MESH_TRIANGULATE_POLYGONS_HPP
#include "balsa/geometry/polygon_mesh/PolygonMesh.hpp"
#include "balsa/geometry/triangle_mesh/earclipping.hpp"

#include <spdlog/spdlog.h>
#include <zipper/utils/format.hpp>
namespace balsa::geometry::polygon_mesh {

// triangulates the polygons in a polygon mesh
template<typename Scalar, int8_t D>
ColVectors<index_type, 3> triangulate_polygons(const polygon_mesh::PolygonMesh<Scalar, D> &pmesh) {
    const auto &pindices = pmesh.polygons;
    size_t poly_added = 0;
    size_t skipped = 0;
    for (size_t j = 0; j < pindices.polygon_count(); ++j) {

        auto offs = pindices.get_polygon_offsets(j);
        int verts = offs(1) - offs(0);
        if (verts < 3) {
            skipped++;
        } else if (verts != 3) {
            poly_added += verts - 3;
        }
    }
    if (skipped == 0 && poly_added == 0) {
        auto r = pindices._buffer.expression().as_std_span();
        using ET = ColVectors<index_type, 3>::extents_type;
        ColVectors<index_type, 3>::const_span_type a(r, ET{ pindices.polygon_count() });
        return a;
    } else {
        index_type total_poly = pindices.polygon_count() + poly_added - skipped;
        ColVectors<index_type, 3> result(total_poly);
        index_type out_index = 0;
        for (size_t j = 0; j < pindices.polygon_count(); ++j) {

            auto p = pindices.get_polygon(j).eval();
            if (p.size() < 3) {
                continue;
            } else if (p.size() == 3) {
                result.col(out_index++) = p;
            } else if (p.size() == 4) {
                result.col(out_index++) = { p(0), p(1), p(2) };
                result.col(out_index++) = { p(0), p(2), p(3) };
            } else {

                zipper::ColVectors<index_type, 3> tris(0);
                if constexpr (D == 3) {
                    auto a = pmesh.vertices.col(p(0));
                    auto b = pmesh.vertices.col(p(1));
                    Matrix<Scalar, 2, 3> UV;
                    auto u = UV.row(0);
                    u = (b - a);
                    u = u / u.norm();
                    // u.normalize();
                    Vector<Scalar, 3> N;
                    for (index_type k = 1; k < p.size(); ++k) {
                        auto c = pmesh.vertices.col(p(k));
                        auto v = UV.row(1);
                        v = (c - a).normalized();
                        N = u.eval().cross(v.eval());
                        if (N.norm() > 1e-2) {
                            break;
                        }
                    }
                    tris = balsa::geometry::triangle_mesh::earclipping(UV * pmesh.vertices, p);
                } else if constexpr (D == 2) {
                    tris = balsa::geometry::triangle_mesh::earclipping(pmesh.vertices, p);
                }
                for (index_type k = 0; k < tris.cols(); ++k) {
                    result.col(out_index++) = tris.col(k);
                }
            }
        }
        return result;
    }
}
}// namespace balsa::geometry::polygon_mesh
#endif
