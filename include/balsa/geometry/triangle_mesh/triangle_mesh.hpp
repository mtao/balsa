#if !defined(BALSA_GEOMETRY_TRIANGLE_MESH_TRIANGLE_MESH_H)
#define BALSA_GEOMETRY_TRIANGLE_MESH_TRIANGLE_MESH_H

#include "balsa/eigen/types.hpp"
#include <tuple>


namespace balsa::geometry::triangle_mesh {
template<typename Scalar = double, int D = 3>
struct TriangleMesh : public std::tuple<eigen::ColVectors<Scalar, D>,// vertices
                                        eigen::ColVectors<int, 3>,// index buffer
                                        eigen::ColVectors<int, 2>>

{
    using Base = std::tuple<eigen::ColVectors<Scalar, D>,// vertices
                            eigen::ColVectors<int, 3>,// index buffer
                            eigen::ColVectors<int, 2>// index buffer
                            >;
    using Base::Base;

    eigen::ColVectors<Scalar, D> &vertices = std::get<0>(*this);
    eigen::ColVectors<int, 3> &triangles = std::get<1>(*this);
    eigen::ColVectors<int, 2> &edges = std::get<2>(*this);
};
}// namespace balsa::geometry::triangle_mesh


#endif
