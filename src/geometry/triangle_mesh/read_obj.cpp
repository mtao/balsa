#include "balsa/geometry/triangle_mesh/read_obj.hpp"
#include "balsa/geometry/polygon_mesh/read_obj_impl.hpp"
#include "balsa/geometry/triangle_mesh/earclipping.hpp"

namespace balsa::geometry::triangle_mesh {
namespace {

    template<typename Scalar, int D>
    balsa::eigen::ColVecs3i poly_to_tri(const polygon_mesh::PolygonMesh<Scalar, D> &pmesh) {
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
}// namespace

template<typename Scalar, int D>
OBJMesh<Scalar, D> read_obj(const std::string &filename) {


    auto pmesh = polygon_mesh::read_obj<Scalar, D>(filename);


    return OBJMesh<Scalar, D>{
        .position = TriangleMesh<Scalar, D>{ pmesh.position.vertices, poly_to_tri(pmesh.position) },
        .texture = TriangleMesh<Scalar, 2>{ pmesh.texture.vertices, poly_to_tri(pmesh.texture) },
        .normal = TriangleMesh<Scalar, D>{ pmesh.normal.vertices, poly_to_tri(pmesh.normal) },
    };
}


OBJMesh<float, 3> read_objF(const std::string &filename) {
    return read_obj<float, 3>(filename);
}
OBJMesh<double, 3> read_objD(const std::string &filename) {
    return read_obj<double, 3>(filename);
}

OBJMesh<float, 2> read_obj2F(const std::string &filename) {
    return read_obj<float, 2>(filename);
}
OBJMesh<double, 2> read_obj2D(const std::string &filename) {
    return read_obj<double, 2>(filename);
}
}// namespace balsa::geometry::triangle_mesh

