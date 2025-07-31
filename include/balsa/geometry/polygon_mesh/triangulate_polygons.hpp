#include "balsa/geometry/polygon_mesh/PolygonMesh.hpp"
#include "balsa/geometry/triangle_mesh/earclipping.hpp"

namespace balsa::geometry::polygon_mesh {

// triangulates the polygons in a polygon mesh
template<typename Scalar, int8_t D>
ColVectors<index_type,3> triangulate_polygons(const polygon_mesh::PolygonMesh<Scalar, D> &pmesh) {
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
         ColVectors<index_type,3> a(pindices.polygon_count());
         a.view().as_span() = pindices._buffer.reshaped.view().as_span();
         return a;
    } else {
        int total_poly = pindices.polygon_count() + poly_added - skipped;
        zipper::ColVecs3i F(3, total_poly);
        int out_index = 0;
        for (size_t j = 0; j < pindices.polygon_count(); ++j) {

            auto p = pindices.get_polygon(j);
            if (p.size() < 3) {
                continue;
            } else if (p.size() == 3) {
                F.col(out_index++) = p;
            } else if (p.size() == 4) {
                F.col(out_index++) = {p(0), p(1), p(2)};
                F.col(out_index++) = {p(0), p(2), p(3)};
            } else {


                zipper::ColVectors<int, 3> F(0);
                if constexpr (D == 3) {
                    auto a = pmesh.vertices.col(p(0));
                    auto b = pmesh.vertices.col(p(1));
                    Eigen::Matrix<Scalar, 2, 3> UV;
                    auto u = UV.row(0) = (b - a).transpose();
                    u.normalize();
                    zipper::Vector<Scalar, 3> N;
                    for (int j = 1; j < p.size(); ++j) {
                        auto c = pmesh.vertices.col(p(j));
                        auto v = UV.row(1) = (c - a).normalized().transpose();
                        N = u.cross(v);
                        if (N.norm() > 1e-2) {
                            break;
                        }
                    }
                    F = balsa::geometry::triangle_mesh::earclipping(UV * pmesh.vertices, p);
                } else if constexpr (D == 2) {
                    F = balsa::geometry::triangle_mesh::earclipping(pmesh.vertices, p);
                }
                for (int j = 0; j < F.cols(); ++j) {
                    auto f = F.col(j);
                    F.col(out_index) = f;
                }
            }
        }
        return F;
    }
}
}// namespace balsa::geometry::polygon_mesh
