#ifndef READ_XYZ_H
#define READ_XYZ_H

#include "balsa/eigen/types.hpp"
#include <array>

namespace balsa::geometry::point_cloud {

//Read an xyz file assuming that it returns points and maybe 3 more values,
//which are returned in two arrays
//If any vertices lack normals the second array is empty
std::array<balsa::eigen::ColVectors<float, 3>, 2> read_xyzF(const std::string &filename);
std::array<balsa::eigen::ColVectors<double, 3>, 2> read_xyzD(const std::string &filename);

}// namespace balsa::geometry::point_cloud


#endif//READ_XYZ_H
