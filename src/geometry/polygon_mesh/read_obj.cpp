#include "balsa/geometry/polygon_mesh/read_obj.hpp"
#include "balsa/geometry/polygon_mesh/read_obj_impl.hpp"
namespace balsa::geometry::polygon_mesh {
// Read an obj file assuming that it returns points and maybe 3 more values,
// which are returned in two arrays
// If any vertices lack normals the second array is empty
OBJMesh<float, 3> read_objF(const std::filesystem::path &filename) {
    return read_obj<float, 3>(filename);
}
OBJMesh<double, 3> read_objD(const std::filesystem::path &filename) {
    return read_obj<double, 3>(filename);
}

OBJMesh<float, 2> read_objF2(const std::filesystem::path &filename) {
    return read_obj<float, 2>(filename);
}
OBJMesh<double, 2> read_objD2(const std::filesystem::path &filename) {
    return read_obj<double, 2>(filename);
}

}// namespace balsa::geometry::polygon_mesh
