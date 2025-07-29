#if !defined(BALSA_GEOMETRY_POLYGON_MESH_POLYGON_MESH_H)
#define BALSA_GEOMETRY_POLYGON_MESH_POLYGON_MESH_H

#include "balsa/zipper/types.hpp"
#include "PolygonBuffer.hpp"
#include "PLCurveBuffer.hpp"
#include <tuple>


namespace balsa::geometry::polygon_mesh {
template<typename Scalar = double, int D = 3>
struct PolygonMesh

{
  public:
    zipper::ColVectors<Scalar, D> vertices;
    PolygonBuffer<int> polygons;
    PLCurveBuffer<int> curves;
};
}// namespace balsa::geometry::polygon_mesh


#endif
