#if !defined(BALSA_GEOMETRY_TRIANGLE_MESH_TRIANGLEMESH_H)
#define BALSA_GEOMETRY_TRIANGLE_MESH_TRIANGLEMESH_H

#include "balsa/tensor_types.hpp"
#include <tuple>


namespace balsa::geometry::triangle_mesh {
template<typename Scalar = double, int8_t D = 3>
struct TriangleMesh {
    ColVectors<Scalar, index_type(D)> vertices;
    ColVectors<index_type, 3> triangles;
    ColVectors<index_type, 2> edges;
};
}// namespace balsa::geometry::triangle_mesh


#endif
