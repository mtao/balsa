#if !defined(BALSA_GEOMETRY_POLYGON_MESH_POLYGON_MESH_H)
#define BALSA_GEOMETRY_POLYGON_MESH_POLYGON_MESH_H

#include "balsa/eigen/types.hpp"
#include "polygon_buffer.hpp"
#include <tuple>


namespace balsa::geometry::polygon_mesh {
template<typename Scalar = double, int D = 3>
struct PolygonMesh : public std::tuple<eigen::ColVectors<Scalar, D>,//vertices
                                       PolygonBuffer<int>,//index buffer
                                       eigen::ColVectors<int, 2>>

{
    using Base = std::tuple<eigen::ColVectors<Scalar, D>,//vertices
                            PolygonBuffer<int>,//index buffer
                            eigen::ColVectors<int, 2>>;

    using Base::Base;
    eigen::ColVectors<Scalar, D> &vertices = std::get<0>(*this);
    PolygonBuffer<int> &polygons = std::get<1>(*this);
    eigen::ColVectors<int, 2> &lines = std::get<2>(*this);
};
}// namespace balsa::geometry::polygon_mesh


#endif
