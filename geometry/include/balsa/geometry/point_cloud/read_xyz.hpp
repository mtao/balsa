#if !defined(BALSA_GEOMETRY_POINT_CLOUD_READ_XYZ_H)
#define BALSA_GEOMETRY_POINT_CLOUD_READ_XYZ_H

#include "balsa/tensor_types.hpp"
#include <array>
#include <string>
#include <vector>

namespace balsa::geometry::point_cloud {

/// Data returned by the XYZ file reader.
///
/// The standard chemistry XYZ format is:
///   <atom_count>
///   <comment line>
///   <species> <x> <y> <z> [<vx> <vy> <vz>]
///   ...
struct XYZData {
    ColVecs3d positions;
    ColVecs3d extra;///< Optional second 3-column block (normals, velocities, etc.)
    std::vector<std::string> species;///< Per-atom element symbols (e.g. "H", "O", "Si")
    std::string comment;///< Comment line from the file header
};

XYZData read_xyzD(const std::string &filename);

// Legacy overload returning just the two column sets (no species/comment).
std::array<balsa::ColVectors<float, 3>, 2> read_xyzF(const std::string &filename);

}// namespace balsa::geometry::point_cloud

#endif
