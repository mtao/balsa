#if !defined(BALSA_GEOMETRY_POINT_CLOUD_WRITE_XYZ_HPP)
#define BALSA_GEOMETRY_POINT_CLOUD_WRITE_XYZ_HPP

#include "balsa/tensor_types.hpp"
#include <string>
#include <vector>

namespace balsa::geometry::point_cloud {

/// Write particles in chemistry XYZ format.
///
/// Output format:
///   <atom_count>
///   <comment>
///   <species> <x> <y> <z> [<ex> <ey> <ez>]
///   ...
///
/// @param filename   Output file path.
/// @param positions  3 x N matrix of particle positions.
/// @param species    Per-particle element symbols. If empty, "X" is used.
/// @param extra      Optional 3 x N second column block (normals, velocities, etc.).
/// @param comment    Comment string for the header line.
void write_xyz(const std::string &filename,
               const ColVecs3d &positions,
               const std::vector<std::string> &species = {},
               const ColVecs3d &extra = {},
               const std::string &comment = "");

}// namespace balsa::geometry::point_cloud

#endif
