#if !defined(BALSA_GEOMETRY_TRIANGLE_MESH_TRIANGLE_MESH_H)
#define BALSA_GEOMETRY_TRIANGLE_MESH_TRIANGLE_MESH_H

#include "balsa/eigen/types.hpp"
#include <tuple>


namespace balsa::geometry::triangle_mesh {
template<typename Scalar = double, int D = 3>
struct TriangleMesh {
    eigen::ColVectors<Scalar, D> vertices;
    eigen::ColVectors<int, 3> triangles;
    eigen::ColVectors<int, 2> edges;
};
}// namespace balsa::geometry::triangle_mesh


#endif
