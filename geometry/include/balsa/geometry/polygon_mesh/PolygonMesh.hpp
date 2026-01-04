#if !defined(BALSA_GEOMETRY_POLYGON_MESH_POLYGON_MESH_H)
#define BALSA_GEOMETRY_POLYGON_MESH_POLYGON_MESH_H

#include "balsa/tensor_types.hpp"
#include "PolygonBuffer.hpp"
#include "PLCurveBuffer.hpp"
#include <tuple>


namespace balsa::geometry::polygon_mesh {
template<typename Scalar = double, int8_t D = 3>
struct PolygonMesh

{
  public:
    ColVectors<Scalar, D> vertices;
    PolygonBuffer<index_type> polygons;
    PLCurveBuffer<index_type> curves;
};
}// namespace balsa::geometry::polygon_mesh


#endif
