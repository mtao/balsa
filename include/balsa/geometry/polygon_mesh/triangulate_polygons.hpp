#include "balsa/geometry/polygon_mesh/polygon_mesh.hpp"
#include "balsa/geometry/triangle_mesh/earclipping.hpp"

namespace balsa::geometry::polygon_mesh {

// triangulates the polygons in a polygon mesh
template<typename Scalar, int D>
balsa::eigen::ColVecs3i triangulate_polygons(const polygon_mesh::PolygonMesh<Scalar, D> &pmesh) {
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
        return pindices._buffer.reshaped(3, pindices.polygon_count());
    } else {
        int total_poly = pindices.polygon_count() + poly_added - skipped;
        eigen::ColVecs3i F(3, total_poly);
        int out_index = 0;
        for (size_t j = 0; j < pindices.polygon_count(); ++j) {

            auto p = pindices.get_polygon(j);
            if (p.size() < 3) {
                continue;
            } else if (p.size() == 3) {
                F.col(out_index++) = p;
            } else if (p.size() == 4) {
                F.col(out_index++) << p(0), p(1), p(2);
                F.col(out_index++) << p(0), p(2), p(3);
            } else {


                eigen::ColVectors<int, 3> F;
                if constexpr (D == 3) {
                    auto a = pmesh.vertices.col(p(0));
                    auto b = pmesh.vertices.col(p(1));
                    Eigen::Matrix<Scalar, 2, 3> UV;
                    auto u = UV.row(0) = (b - a).transpose();
                    u.normalize();
                    eigen::Vector<Scalar, 3> N;
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
                    F.col(out_index) << f(0), f(1), f(2);
                }
            }
        }
        return F;
    }
}
}// namespace balsa::geometry::polygon_mesh
