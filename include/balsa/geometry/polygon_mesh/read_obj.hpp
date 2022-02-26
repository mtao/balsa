#if !defined(BALSA_GEOMETRY_POLYGON_MESH_READ_OBJ_H)
#define BALSA_GEOMETRY_POLYGON_MESH_READ_OBJ_H

#include "polygon_mesh.hpp"

namespace balsa::geometry::polygon_mesh {


// OBJ meshes can hold multiple buffers
template<typename Scalar = double, int D = 3>
struct OBJMesh {
    PolygonMesh<Scalar, D> position;
    PolygonMesh<Scalar, 2> texture;
    PolygonMesh<Scalar, D> normal;
};

//Read an obj file assuming that it returns points and maybe 3 more values,
//which are returned in two arrays
//If any vertices lack normals the second array is empty
OBJMesh<float, 3> read_objF(const std::string &filename);
OBJMesh<double, 3> read_objD(const std::string &filename);

OBJMesh<float, 2> read_objF2(const std::string &filename);
OBJMesh<double, 2> read_objD2(const std::string &filename);


}// namespace balsa::geometry::polygon_mesh


#endif//READ_OBJ_H
