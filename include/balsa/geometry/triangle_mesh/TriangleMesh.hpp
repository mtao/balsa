#if !defined(BALSA_GEOMETRY_TRIANGLE_MESH_TRIANGLEMESH_H)
#define BALSA_GEOMETRY_TRIANGLE_MESH_TRIANGLEMESH_H

#include "balsa/zipper/types.hpp"
#include <tuple>


namespace balsa::geometry::triangle_mesh {
template<typename Scalar = double, int D = 3>
struct TriangleMesh {
    zipper::ColVectors<Scalar, D> vertices;
    zipper::ColVectors<int, 3> triangles;
    zipper::ColVectors<int, 2> edges;
};
}// namespace balsa::geometry::triangle_mesh


#endif
