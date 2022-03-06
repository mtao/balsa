#include "balsa/geometry/triangle_mesh/read_obj.hpp"
#include "balsa/geometry/polygon_mesh/read_obj_impl.hpp"
#include "balsa/geometry/polygon_mesh/triangulate_polygons.hpp"

namespace balsa::geometry::triangle_mesh {
namespace {
    eigen::ColVecs2i edges_from_curves(const polygon_mesh::PLCurveBuffer<int> &curves) {

        int curve_count = curves.curve_count();
        if (curve_count == 0) {
            return {};
        }
        int vertex_count = curves._buffer.size();
        int edge_count = vertex_count - curve_count;


        eigen::ColVecs2i E(2, edge_count);
        int index = 0;
        for (int j = 0; j < curve_count; ++j) {
            auto c = curves.get_curve(j);
            for (int k = 0; k < c.size() - 1; ++k) {
                E.col(index++) = c.segment<2>(k);
            }
        }


        return E;
    }

}// namespace

template<typename Scalar, int D>
OBJMesh<Scalar, D> read_obj(const std::filesystem::path &filename) {


    auto pmesh = polygon_mesh::read_obj<Scalar, D>(filename);


    return OBJMesh<Scalar, D>{
        .position = TriangleMesh<Scalar, D>{ pmesh.position.vertices, polygon_mesh::triangulate_polygons(pmesh.position), edges_from_curves(pmesh.position.curves) },
        .texture = TriangleMesh<Scalar, 2>{ pmesh.texture.vertices, polygon_mesh::triangulate_polygons(pmesh.texture), edges_from_curves(pmesh.texture.curves) },
        .normal = TriangleMesh<Scalar, D>{ pmesh.normal.vertices, polygon_mesh::triangulate_polygons(pmesh.normal), edges_from_curves(pmesh.normal.curves) },
    };
}


OBJMesh<float, 3> read_objF(const std::filesystem::path &filename) {
    return read_obj<float, 3>(filename);
}
OBJMesh<double, 3> read_objD(const std::filesystem::path &filename) {
    return read_obj<double, 3>(filename);
}

OBJMesh<float, 2> read_obj2F(const std::filesystem::path &filename) {
    return read_obj<float, 2>(filename);
}
OBJMesh<double, 2> read_obj2D(const std::filesystem::path &filename) {
    return read_obj<double, 2>(filename);
}
}// namespace balsa::geometry::triangle_mesh

