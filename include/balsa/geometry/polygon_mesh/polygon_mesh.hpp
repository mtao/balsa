#if !defined(BALSA_GEOMETRY_POLYGON_MESH_POLYGON_MESH_H)
#define BALSA_GEOMETRY_POLYGON_MESH_POLYGON_MESH_H

#include "balsa/eigen/types.hpp"
#include "polygon_buffer.hpp"
#include "plcurve_buffer.hpp"
#include <tuple>


namespace balsa::geometry::polygon_mesh {
template<typename Scalar = double, int D = 3>
struct PolygonMesh

{
  public:
    eigen::ColVectors<Scalar, D> vertices;
    PolygonBuffer<int> polygons;
    PLCurveBuffer<int> curves;
};
}// namespace balsa::geometry::polygon_mesh


#endif
