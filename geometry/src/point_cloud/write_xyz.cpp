#include "balsa/geometry/point_cloud/write_xyz.hpp"
#include <fstream>
#include <format>
#include <stdexcept>

namespace balsa::geometry::point_cloud {

void write_xyz(const std::string &filename,
               const ColVecs3d &positions,
               const std::vector<std::string> &species,
               const ColVecs3d &extra,
               const std::string &comment) {
    std::ofstream ofs(filename);
    if (!ofs) {
        throw std::runtime_error("Failed to open XYZ file for writing: " + filename);
    }

    const auto n = positions.cols();
    const bool has_species = !species.empty();
    const bool has_extra = extra.cols() > 0;

    // Line 1: atom count
    ofs << n << '\n';

    // Line 2: comment
    ofs << comment << '\n';

    // Data lines: <species> <x> <y> <z> [<ex> <ey> <ez>]
    for (decltype(positions.cols()) i = 0; i < n; ++i) {
        const auto &s = has_species ? species[i] : std::string("X");
        auto p = positions.col(i);
        ofs << std::format("{} {:.10g} {:.10g} {:.10g}", s, p(0), p(1), p(2));
        if (has_extra) {
            auto e = extra.col(i);
            ofs << std::format(" {:.10g} {:.10g} {:.10g}", e(0), e(1), e(2));
        }
        ofs << '\n';
    }
}

}// namespace balsa::geometry::point_cloud
