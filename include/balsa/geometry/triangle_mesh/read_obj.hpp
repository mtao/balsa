#if !defined(BALSA_GEOMETRY_TRIANGLE_MESH_READ_OBJ_H)
#define BALSA_GEOMETRY_TRIANGLE_MESH_READ_OBJ_H

#include "triangle_mesh.hpp"

namespace balsa::geometry::triangle_mesh {


// OBJ meshes can hold multiple buffers
template<typename Scalar = double, int D = 3>
struct OBJMesh {
    TriangleMesh<Scalar, D> position;
    TriangleMesh<Scalar, 2> texture;
    TriangleMesh<Scalar, D> normal;
};

//Read an obj file assuming that it returns points and maybe 3 more values,
//which are returned in two arrays
//If any vertices lack normals the second array is empty
OBJMesh<float, 3> read_objF(const std::string &filename);
OBJMesh<double, 3> read_objD(const std::string &filename);

OBJMesh<float, 2> read_objF2(const std::string &filename);
OBJMesh<double, 2> read_objD2(const std::string &filename);


}// namespace balsa::geometry::triangle_mesh


#endif//READ_OBJ_H
