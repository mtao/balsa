#include "balsa/geometry/polygon_mesh/PolygonMesh.hpp"
#include "balsa/geometry/triangle_mesh/earclipping.hpp"

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
        ColVectors<index_type, 3> a(pindices.polygon_count());
        auto s = a.view().accessor().as_std_span();
        auto r = pindices._buffer.view().accessor().as_std_span();
        std::copy(r.begin(), r.end(), s.begin());

        return a;
    } else {
        index_type total_poly = pindices.polygon_count() + poly_added - skipped;
        ColVectors<index_type, 3> F(total_poly);
        index_type out_index = 0;
        for (size_t j = 0; j < pindices.polygon_count(); ++j) {

            auto p = pindices.get_polygon(j).eval();
            if (p.size() < 3) {
                continue;
            } else if (p.size() == 3) {
                F.col(out_index++) = p;
            } else if (p.size() == 4) {
                F.col(out_index++) = { p(0), p(1), p(2) };
                F.col(out_index++) = { p(0), p(2), p(3) };
            } else {


                zipper::ColVectors<index_type, 3> F(0);
                if constexpr (D == 3) {
                    auto a = pmesh.vertices.col(p(0));
                    auto b = pmesh.vertices.col(p(1));
                    Matrix<Scalar, 2, 3> UV;
                    auto u = UV.row(0);
                    u = (b - a);
                    u = u / u.norm();
                    // u.normalize();
                    Vector<Scalar, 3> N;
                    for (index_type j = 1; j < p.size(); ++j) {
                        auto c = pmesh.vertices.col(p(j));
                        auto v = UV.row(1);
                        v = (c - a).normalized();
                        N = u.cross(v);
                        if (N.norm() > 1e-2) {
                            break;
                        }
                    }
                    F = balsa::geometry::triangle_mesh::earclipping(UV * pmesh.vertices, p);
                } else if constexpr (D == 2) {
                    F = balsa::geometry::triangle_mesh::earclipping(pmesh.vertices, p);
                }
                for (index_type j = 0; j < F.cols(); ++j) {
                    auto f = F.col(j);
                    F.col(out_index) = f;
                }
            }
        }
        return F;
    }
}
}// namespace balsa::geometry::polygon_mesh
